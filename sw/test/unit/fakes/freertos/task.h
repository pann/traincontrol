#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define pdPASS  1
#define pdFAIL  0
#define tskIDLE_PRIORITY 0

typedef void (*TaskFunction_t)(void *);

static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }
static inline void vTaskDelete(TaskHandle_t task) { (void)task; }

/* xTaskCreate fake — declared here, implemented in fakes_state.c */
BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char *pcName,
                       uint32_t usStackDepth, void *pvParameters,
                       UBaseType_t uxPriority, TaskHandle_t *pxCreatedTask);
