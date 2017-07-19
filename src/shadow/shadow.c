#include "iot_import.h"

#include "utils_list.h"
#include "utils_debug.h"
#include "utils_error.h"
#include "mqtt_client.h"
#include "lite/lite-utils.h"
#include "shadow.h"
#include "shadow_common.h"
#include "shadow_update.h"
#include "shadow_delta.h"


//check return code
#define CHECK_RETURN_CODE(ret_code) \
    do{ \
        if (ret_code < 0) { \
            return ret_code; \
        } \
    }while(0);


//check return code of snprintf
#define CHECK_SNPRINTF_RET(ret_code, buf_len) \
    do{ \
        if ((ret_code) < 0) { \
            return FAIL_RETURN; \
        } else if ((ret_code) >= (buf_len)) { \
            return ERROR_NO_ENOUGH_MEM; \
        } \
    }while(0);


static void ads_handle_expire(aliot_shadow_pt pshadow)
{
    ads_update_wait_ack_list_handle_expire(pshadow);
}


//This function will be called back when message published to topic(/shadow/get/) arrives.
static void aliot_shadow_callback_get(aliot_shadow_pt pshadow, void *pclient, aliot_mqtt_event_msg_pt msg)
{
    const char *pname;
    int val_type;

    aliot_mqtt_topic_info_pt topic_info = (aliot_mqtt_topic_info_pt)msg->msg;

    log_debug("topic=%.*s", topic_info->topic_len, topic_info->ptopic);
    log_debug("data of topic=%.*s", topic_info->payload_len, (char *)topic_info->payload);

    //update time if there is 'timestamp' key in JSON string
    pname = LITE_json_value_of("timestamp", topic_info->payload);
    if (NULL != pname) {
        ads_common_update_time(pshadow, atoi(pname));
    }
    LITE_free(pname);

    //update 'version' if there is 'version' key in JSON string
    pname = LITE_json_value_of("version", topic_info->payload);
    if (NULL != pname) {
        ads_common_update_version(pshadow, atoi(pname));
        LITE_free(pname);
    }

    //get 'method'
    pname = LITE_json_value_of("method", topic_info->payload);
    log_debug("pname(%d) = %s", strlen(pname), pname);
    if (NULL == pname) {
        log_err("Invalid JSON document: not 'method' key");
    } else if ((strlen("control") == strlen(pname)) && !strcmp(pname, "control")) {
        //call delta handle function
        log_debug("receive 'control' method");

        aliot_shadow_delta_entry(
                    pshadow,
                    topic_info->payload,
                    topic_info->payload_len);
        LITE_free(pname);
    } else if ((strlen("reply") == strlen(pname)) && !strcmp(pname, "reply")) {
        //call update ACK handle function.
        log_debug("receive 'reply' method");
        ads_update_wait_ack_list_handle_response(
                    pshadow,
                    topic_info->payload,
                    topic_info->payload_len);
        LITE_free(pname);
    } else {
        log_err("Invalid 'method' key");
        LITE_free(pname);
    }

    log_debug("End of method handle");
}

static aliot_err_t aliot_shadow_subcribe_get(aliot_shadow_pt pshadow)
{
    if (NULL == pshadow->inner_data.ptopic_get) {
        pshadow->inner_data.ptopic_get = ads_common_generate_topic_name(pshadow, "get");
        if (NULL == pshadow->inner_data.ptopic_get) {
            return FAIL_RETURN;
        }
    }

    return aliot_mqtt_subscribe(pshadow->mqtt,
                                 pshadow->inner_data.ptopic_get,
                                 ALIOT_MQTT_QOS1,
                                 (aliot_mqtt_event_handle_func_fpt)aliot_shadow_callback_get,
                                 pshadow);
}


aliot_err_t aliot_shadow_update_format_init(void *pshadow,
                format_data_pt pformat,
                char *buf,
                uint16_t size)
{
    return ads_common_format_init((aliot_shadow_pt)pshadow, pformat, buf, size, "update", "\"state\":{\"reported\":{");
}


aliot_err_t aliot_shadow_update_format_add(void *pshadow,
                format_data_pt pformat,
                aliot_shadow_attr_pt pattr)
{
    return ads_common_format_add((aliot_shadow_pt)pshadow, pformat, pattr->pattr_name, pattr->pattr_data, pattr->attr_type);
}


aliot_err_t aliot_shadow_update_format_finalize(void *pshadow, format_data_pt pformat)
{
    return ads_common_format_finalize((aliot_shadow_pt)pshadow, pformat, "}}");
}


aliot_err_t aliot_shadow_update_asyn(
                void *handle,
                char *data,
                size_t data_len,
                uint16_t timeout_s,
                aliot_update_cb_fpt cb_fpt,
                void *pcontext)
{
    int rc = SUCCESS_RETURN;
    aliot_update_ack_wait_list_pt pelement;
    const char *ptoken;
    aliot_shadow_pt pshadow = (aliot_shadow_pt)handle;

    if ((NULL == handle) || (NULL == data)) {
        return NULL_VALUE_ERROR;
    }

    if (!aliot_mqtt_check_state_normal(pshadow->mqtt)) {
        log_err("The MQTT connection must be established before UPDATE data.");
        return ERROR_SHADOW_INVALID_STATE;
    }

    /*Add to callback list */

    log_debug("data(%d) = %s", data_len, data);
    ptoken = LITE_json_value_of("clientToken", data);

    ALIOT_ASSERT(NULL != ptoken, "Token should always exist.");

    pelement = aliot_shadow_update_wait_ack_list_add(pshadow, ptoken, strlen(ptoken), cb_fpt, pcontext, timeout_s);
    if (NULL == pelement) {
        LITE_free(ptoken);
        return ERROR_SHADOW_WAIT_LIST_OVERFLOW;
    }
    LITE_free(ptoken);

    if ((rc = ads_common_publish2update(pshadow, data, data_len)) < 0) {
        aliot_shadow_update_wait_ack_list_remove(pshadow, pelement);
        return rc;
    }

    return SUCCESS_RETURN;
}



static void aliot_update_ack_cb(
                void *pcontext,
                aliot_shadow_ack_code_t ack_code,
                const char *ack_msg, //NOTE: NOT a string.
                uint32_t ack_msg_len)
{
    log_debug("ack_code=%d", ack_code);

    if (NULL != ack_msg) {
        log_debug("ack_msg=%.*s", ack_msg_len, ack_msg);
    } else {
        log_debug("ack_msg is NULL");
    }

    *((aliot_shadow_ack_code_t *)pcontext) = ack_code;
}


aliot_err_t aliot_shadow_update(
                void *handle,
                char *data,
                uint32_t data_len,
                uint16_t timeout_s)
{
    aliot_shadow_ack_code_t ack_update = ALIOT_SHADOW_ACK_NONE;
    aliot_shadow_pt pshadow = (aliot_shadow_pt)handle;

    if ((NULL == pshadow) || (NULL == data)) {
        return NULL_VALUE_ERROR;
    }

    if (!aliot_mqtt_check_state_normal(pshadow->mqtt)) {
        log_err("The MQTT connection must be established before UPDATE data.");
        return ERROR_SHADOW_INVALID_STATE;
    }

    //update asynchronously
    aliot_shadow_update_asyn(pshadow, data, data_len, timeout_s, aliot_update_ack_cb, &ack_update);

    //wait ACK
    while (ALIOT_SHADOW_ACK_NONE == ack_update) {
        aliot_shadow_yield(pshadow, 200);
    }

    if ((ALIOT_SHADOW_ACK_SUCCESS == ack_update)
        || (ALIOT_SHADOW_ACK_ERR_SHADOW_DOCUMENT_IS_NULL == ack_update)) {
        //It is not the error that device shadow document is null
        log_info("update success.");
        return SUCCESS_RETURN;
    } else if (ALIOT_SHADOW_ACK_TIMEOUT == ack_update) {
        log_info("update timeout.");
        return ERROR_SHADOW_UPDATE_TIMEOUT;
    } else {
        log_info("update negative ack.");
        return ERROR_SHADOW_UPDATE_NACK;
    }
}


aliot_err_t aliot_shadow_sync(void *handle)
{
#define SHADOW_SYNC_MSG_SIZE      (256)

    aliot_err_t ret;
    void *buf;
    format_data_t format;
    aliot_shadow_pt pshadow = (aliot_shadow_pt)handle;

    log_info("Device Shadow sync start.");

    buf = aliot_platform_malloc(SHADOW_SYNC_MSG_SIZE);
    if (NULL == buf) {
        log_err("Device Shadow sync failed");
        return ERROR_NO_MEM;
    }

    ads_common_format_init(pshadow, &format, buf, SHADOW_SYNC_MSG_SIZE, "get", NULL);
    ads_common_format_finalize(pshadow, &format, NULL);

    ret = aliot_shadow_update(pshadow, format.buf, format.offset, 10);
    if (SUCCESS_RETURN == ret) {
        log_info("Device Shadow sync success.");
    } else {
        log_info("Device Shadow sync failed.");
    }

    aliot_platform_free(buf);
    aliot_platform_msleep(1000);

    return ret;

#undef SHADOW_SYNC_MSG_SIZE
}


void ads_event_handle(void *pcontext, void *pclient, aliot_mqtt_event_msg_pt msg)
{
    aliot_shadow_pt pshadow = (aliot_shadow_pt)pcontext;
    uint32_t packet_id = (uint32_t)msg->msg;
    aliot_mqtt_topic_info_pt topic_info = (aliot_mqtt_topic_info_pt)msg->msg;

    switch (msg->event_type)
    {
    case ALIOT_MQTT_EVENT_SUBCRIBE_SUCCESS:
        log_info("subscribe success, packet-id=%u", packet_id);
        if (pshadow->inner_data.sync_status == packet_id) {
            pshadow->inner_data.sync_status = 0;
        }
        break;

    case ALIOT_MQTT_EVENT_SUBCRIBE_TIMEOUT:
        log_info("subscribe wait ack timeout, packet-id=%u", packet_id);
        if (pshadow->inner_data.sync_status == packet_id) {
            pshadow->inner_data.sync_status = -1;
        }
        break;

    case ALIOT_MQTT_EVENT_SUBCRIBE_NACK:
        log_info("subscribe nack, packet-id=%u", packet_id);
        if (pshadow->inner_data.sync_status == packet_id) {
            pshadow->inner_data.sync_status = -1;
        }
        break;

    case ALIOT_MQTT_EVENT_PUBLISH_RECVEIVED:
        log_info("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                topic_info->topic_len,
                topic_info->ptopic,
                topic_info->payload_len,
                topic_info->payload);
        break;

    default:
        //log_info("Should NOT arrive here.");
        break;
    }
}

void *aliot_shadow_construct(aliot_shadow_para_pt pparams)
{
    int rc = 0;
    aliot_shadow_pt pshadow = NULL;

    //initialize shadow
    if (NULL == (pshadow = aliot_platform_malloc(sizeof(aliot_shadow_t)))) {
        log_err("Not enough memory");
        return NULL;
    }
    memset(pshadow, 0x0, sizeof(aliot_shadow_t));

    if (NULL == (pshadow->mutex = aliot_platform_mutex_create())) {
        log_err("create mutex failed");
        goto do_exit;
    }

    pparams->mqtt.handle_event.h_fp = ads_event_handle;
    pparams->mqtt.handle_event.pcontext = pshadow;

    //construct MQTT client
    if (NULL == (pshadow->mqtt = aliot_mqtt_construct(&pparams->mqtt))) {
        log_err("construct MQTT failed");
        goto do_exit;
    }

    rc = aliot_shadow_subcribe_get(pshadow);
    if (rc < 0) {
        log_err("subscribe 'get' topic fialed, rc=%d", rc);
        goto do_exit;
    }

    pshadow->inner_data.sync_status = rc;

    while(rc == pshadow->inner_data.sync_status) {
        aliot_shadow_yield(pshadow, 100);
    }

    if (0 == pshadow->inner_data.sync_status) {
        log_info("Sync device data successfully");
    } else {
        log_info("Sync device data failed");
    }


    pshadow->inner_data.attr_list = list_new();
    if (NULL == pshadow->inner_data.attr_list) {
        log_err("new list failed");
        goto do_exit;
    }

    return pshadow;

do_exit:
    aliot_shadow_deconstruct(pshadow);

    return NULL;
}


void aliot_shadow_yield(void *handle, uint32_t timeout)
{
    aliot_shadow_pt pshadow = (aliot_shadow_pt)handle;
    aliot_mqtt_yield(pshadow->mqtt, timeout);
    ads_handle_expire(pshadow);
}


aliot_err_t aliot_shadow_deconstruct(void *handle)
{
    aliot_shadow_pt pshadow = (aliot_shadow_pt) handle;

    if (NULL != pshadow->mqtt) {
        if (NULL != pshadow->inner_data.ptopic_get) {
            aliot_mqtt_unsubscribe(pshadow->mqtt, pshadow->inner_data.ptopic_get);
        }

        aliot_platform_msleep(2000);
        aliot_mqtt_deconstruct(pshadow->mqtt);
    }

    if (NULL != pshadow->inner_data.ptopic_get) {
        aliot_platform_free(pshadow->inner_data.ptopic_get);
    }

    if (NULL != pshadow->inner_data.ptopic_update) {
        aliot_platform_free(pshadow->inner_data.ptopic_update);
    }

    if (NULL != pshadow->inner_data.attr_list) {
        list_destroy(pshadow->inner_data.attr_list);
    }

    if (NULL != pshadow->mutex) {
        aliot_platform_mutex_destroy(pshadow->mutex);
    }

    aliot_platform_free(handle);

    return SUCCESS_RETURN;
}


aliot_err_t aliot_shadow_register_attribute(void *handle, aliot_shadow_attr_pt pattr)
{
    //check if already registered
    if (ads_common_check_attr_existence((aliot_shadow_pt)handle, pattr)) {
        return ERROR_SHADOW_ATTR_EXIST;
    }

    if (SUCCESS_RETURN != ads_common_register_attr((aliot_shadow_pt)handle, pattr)) {
        return FAIL_RETURN;
    }

    return SUCCESS_RETURN;
}


//Remove attribute from Device Shadow in cloud by delete method.
aliot_err_t aliot_shadow_delete_attribute(void *handle, aliot_shadow_attr_pt pattr)
{
#define SHADOW_DELETE_MSG_SIZE      (256)

    aliot_err_t ret;
    void *buf;
    format_data_t format;
    aliot_shadow_pt pshadow = (aliot_shadow_pt) handle;

    if (!ads_common_check_attr_existence(pshadow, pattr)) {
        return ERROR_SHADOW_ATTR_NO_EXIST;
    }

    buf = aliot_platform_malloc(SHADOW_DELETE_MSG_SIZE);
    if (NULL == buf) {
        return ERROR_NO_MEM;
    }

    ads_common_format_init(pshadow, &format, buf, SHADOW_DELETE_MSG_SIZE, "delete", ",\"state\":{\"reported\":{");
    ads_common_format_add(pshadow, &format, pattr->pattr_name, NULL, ALIOT_SHADOW_NULL);
    ads_common_format_finalize(pshadow, &format, "}}");

    ret = aliot_shadow_update(pshadow, format.buf, format.offset, 10);
    if (SUCCESS_RETURN != ret) {
        aliot_platform_free(buf);
        return ret;
    }

    aliot_platform_free(buf);

    return ads_common_remove_attr(pshadow, pattr);

#undef SHADOW_DELETE_MSG_SIZE
}