/**
 * Generated by anjay_codegen.py on 2023-01-07 18:41:01
 *
 * LwM2M Object: Air quality
 * ID: 3428, URN: urn:oma:lwm2m:ext:3428, Optional, Multiple
 *
 * The uCIFI air quality sensor reports measurement required to calculate
 * Air Quality Index (AIQ. It also provides resources for average
 * calculation as per the IQ index calculation.
 */
#include <assert.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_memory.h>

#include "pasco2.h"
#include "oled.h"

#if CONFIG_ANJAY_CLIENT_AIR_QUALITY_SENSOR

/**
 * Air quality object ID
 */
#define OID_AIR_QUALITY 3428

/**
 * CO2: R, Single, Optional
 * type: float, range: N/A, unit: ppm
 * Level of carbon dioxide measured by the air quality sensor.
 */
#define RID_CO2 17

/**
 * CO2 1 hour average: R, Single, Optional
 * type: float, range: N/A, unit: ppm
 * Average level of carbon dioxide measured by the sensor during the last
 * 1 hour.
 */
#define RID_CO2_1_HOUR_AVERAGE 18

typedef struct air_quality_instance_struct {
    uint16_t air_quality[PASCO2_NUMBER_OF_MEASURMENTS_PER_HOUR];
    uint32_t air_quality_avr;
    uint16_t current_measurment_array_field;
    bool measurment_buffer_filled;

    uint8_t heading_id;
    uint8_t meas_text_id;
    uint8_t ppm_id;
    char meas_txt[OLED_MAX_CHAR_PER_LINE + 1]; // +1 for null terminator
} air_quality_instance_t;

typedef struct air_quality_object_struct {
    const anjay_dm_object_def_t *def;
    pthread_mutex_t mutex;
    air_quality_instance_t instances[1];
} air_quality_object_t;

static inline air_quality_object_t *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, air_quality_object_t, def);
}

static int list_instances(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_dm_list_ctx_t *ctx) {
    (void) anjay;
    air_quality_object_t *obj = get_obj(obj_ptr);
    pthread_mutex_lock(&obj->mutex);
    for (anjay_iid_t iid = 0; iid < AVS_ARRAY_SIZE(obj->instances); iid++) {
        anjay_dm_emit(ctx, iid);
    }
    pthread_mutex_unlock(&obj->mutex);
    return 0;
}

static int instance_reset(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid) {
    (void) anjay;

    air_quality_object_t *obj = get_obj(obj_ptr);
    assert(obj);

    pthread_mutex_lock(&obj->mutex);
    assert(iid < AVS_ARRAY_SIZE(obj->instances));
    air_quality_instance_t *inst = &obj->instances[iid];

    inst->measurment_buffer_filled = false;
    inst->current_measurment_array_field = 0;
    inst->air_quality_avr = 0;
    pthread_mutex_unlock(&obj->mutex);
    return 0;
}

static int list_resources(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_dm_resource_list_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    anjay_dm_emit_res(ctx, RID_CO2,
                      ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_CO2_1_HOUR_AVERAGE,
                      ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
    return 0;
}

static int resource_read(anjay_t *anjay,
                         const anjay_dm_object_def_t *const *obj_ptr,
                         anjay_iid_t iid,
                         anjay_rid_t rid,
                         anjay_riid_t riid,
                         anjay_output_ctx_t *ctx) {
    (void) anjay;

    air_quality_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    pthread_mutex_lock(&obj->mutex);
    int result = ANJAY_ERR_NOT_FOUND;
    assert(iid < AVS_ARRAY_SIZE(obj->instances));
    air_quality_instance_t *inst = &obj->instances[iid];

    switch (rid) {
    case RID_CO2:
        assert(riid == ANJAY_ID_INVALID);
        result = anjay_ret_double(ctx, (double)inst->air_quality[inst->current_measurment_array_field - 1]);
        break;

    case RID_CO2_1_HOUR_AVERAGE:
        assert(riid == ANJAY_ID_INVALID);
        result = anjay_ret_double(ctx, (double)inst->air_quality_avr);
        break;

    default:
        result = ANJAY_ERR_NOT_FOUND;
    }
    pthread_mutex_unlock(&obj->mutex);
    return result;
}

static const anjay_dm_object_def_t OBJ_DEF = {
    .oid = OID_AIR_QUALITY,
    .handlers = {
        .list_instances = list_instances,
        .instance_reset = instance_reset,

        .list_resources = list_resources,
        .resource_read = resource_read,
        .resource_write = anjay_dm_transaction_NOOP,

        .transaction_begin = anjay_dm_transaction_NOOP,
        .transaction_validate = anjay_dm_transaction_NOOP,
        .transaction_commit = anjay_dm_transaction_NOOP,
        .transaction_rollback = anjay_dm_transaction_NOOP
    }
};

const anjay_dm_object_def_t **air_quality_object_create(void) {
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr)) {
        return NULL;
    }
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    air_quality_object_t *obj = (air_quality_object_t *) avs_calloc(1, sizeof(air_quality_object_t));
    if (!obj) {
        return NULL;
    }
    obj->def = &OBJ_DEF;

    if (pthread_mutex_init(&obj->mutex, &attr)) {
        pthread_mutexattr_destroy(&attr);
        avs_free(obj);
        return NULL;
    }

    pthread_mutexattr_destroy(&attr);
    pthread_mutex_lock(&obj->mutex);

    air_quality_instance_t *inst = &obj->instances[0];

    OLED_createTextField(&inst->heading_id, 0U, 0U, "CO2: ", 1U, false);
    snprintf(inst->meas_txt, OLED_MAX_CHAR_PER_LINE + 1, "---");
    OLED_createTextField(&inst->meas_text_id, 30U, 0U, inst->meas_txt, 1U, false);

    OLED_update();
    pthread_mutex_unlock(&obj->mutex);

    return &obj->def;
}

void air_quality_object_release(const anjay_dm_object_def_t **def) {
    if (def) {
        air_quality_object_t *obj = get_obj(def);
        pthread_mutex_lock(&obj->mutex);
        air_quality_instance_t *inst = &obj->instances[0];

        OLED_deleteObject(inst->heading_id);
        OLED_deleteObject(inst->meas_text_id);

        pthread_mutex_unlock(&obj->mutex);
        pthread_mutex_destroy(&obj->mutex);
        avs_free(obj);
    }
}

void air_quality_update_measurment_val(const anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr, const uint16_t val) {
    air_quality_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    pthread_mutex_lock(&obj->mutex);
    air_quality_instance_t *inst = &obj->instances[0];

    snprintf(inst->meas_txt, OLED_MAX_CHAR_PER_LINE + 1, "%"PRIu16, val);

    inst->air_quality[inst->current_measurment_array_field++] = val;

    if (!(inst->current_measurment_array_field %= PASCO2_NUMBER_OF_MEASURMENTS_PER_HOUR)) {
        inst->measurment_buffer_filled = true;    // the entire table is filled with measurements
    }

    inst->air_quality_avr = 0;
    for (uint32_t meas = 0U; meas < (inst->measurment_buffer_filled ? PASCO2_NUMBER_OF_MEASURMENTS_PER_HOUR : inst->current_measurment_array_field); meas++) {
        inst->air_quality_avr += inst->air_quality[meas];
    }
    inst->air_quality_avr /= (inst->measurment_buffer_filled ? PASCO2_NUMBER_OF_MEASURMENTS_PER_HOUR : inst->current_measurment_array_field);

    anjay_notify_changed((anjay_t *) anjay, OID_AIR_QUALITY, 0,
                             RID_CO2);

    anjay_notify_changed((anjay_t *) anjay, OID_AIR_QUALITY, 0,
                             RID_CO2_1_HOUR_AVERAGE);
    pthread_mutex_unlock(&obj->mutex);
}

#else   //ANJAY_CLIENT_AIR_QUALITY_SENSOR

const anjay_dm_object_def_t **air_quality_object_create(void) {
    return NULL;
}

void air_quality_object_release(const anjay_dm_object_def_t **def) {
    (void) def;
}

void air_quality_update_measurment_val(const anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr, const uint16_t val) {
    (void) anjay;
    (void) obj_ptr;
    (void) val;
}

#endif  //ANJAY_CLIENT_AIR_QUALITY_SENSOR
