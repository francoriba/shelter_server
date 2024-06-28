#pragma once

#include "../lib/cJSON/include/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
// Define structures for food and medicine supplies
typedef struct
{
    int meat;
    int vegetables;
    int fruits;
    int water;
} FoodSupply;

typedef struct
{
    int antibiotics;
    int analgesics;
    int bandages;
} MedicineSupply;

// Function to initialize shared memory for supplies data
void init_shared_memory_supplies();

// Function to get a pointer to the shared food supply data
FoodSupply* get_food_supply();

// Function to get a pointer to the shared medicine supply data
MedicineSupply* get_medicine_supply();

void update_supplies_from_json(FoodSupply* food_supply, MedicineSupply* medicine_supply, cJSON* json);
