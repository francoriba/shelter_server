#include "supplies_module.h"

#define FOOD_SUPPLY_KEY 1234
#define MEDICINE_SUPPLY_KEY 5678

// Function to initialize shared memory for supplies data
void init_shared_memory_supplies()
{
    // Create shared memory for food supply
    int food_shmid = shmget(FOOD_SUPPLY_KEY, sizeof(FoodSupply), IPC_CREAT | 0666);
    if (food_shmid == -1)
    {
        perror("shmget for food supply");
        return;
    }

    // Attach shared memory for food supply
    FoodSupply* food_supply = (FoodSupply*)shmat(food_shmid, NULL, 0);
    if (food_supply == (FoodSupply*)(-1))
    {
        perror("shmat for food supply");
        return;
    }

    // Initialize food supply data
    food_supply->meat = 0;
    food_supply->vegetables = 0;
    food_supply->fruits = 0;
    food_supply->water = 0;

    // Create shared memory for medicine supply
    int medicine_shmid = shmget(MEDICINE_SUPPLY_KEY, sizeof(MedicineSupply), IPC_CREAT | 0666);
    if (medicine_shmid == -1)
    {
        perror("shmget for medicine supply");
        return;
    }

    // Attach shared memory for medicine supply
    MedicineSupply* medicine_supply = (MedicineSupply*)shmat(medicine_shmid, NULL, 0);
    if (medicine_supply == (MedicineSupply*)(-1))
    {
        perror("shmat for medicine supply");
        return;
    }

    // Initialize medicine supply data
    medicine_supply->antibiotics = 0;
    medicine_supply->analgesics = 0;
    medicine_supply->bandages = 0;
}

// Function to get a pointer to the shared food supply data
FoodSupply* get_food_supply()
{
    // Get shared memory for food supply
    int food_shmid = shmget(FOOD_SUPPLY_KEY, sizeof(FoodSupply), 0666);
    if (food_shmid == -1)
    {
        perror("shmget for food supply");
        return NULL;
    }

    // Attach shared memory for food supply
    FoodSupply* food_supply = (FoodSupply*)shmat(food_shmid, NULL, 0);
    if (food_supply == (FoodSupply*)(-1))
    {
        perror("shmat for food supply");
        return NULL;
    }

    return food_supply;
}

// Function to get a pointer to the shared medicine supply data
MedicineSupply* get_medicine_supply()
{
    // Get shared memory for medicine supply
    int medicine_shmid = shmget(MEDICINE_SUPPLY_KEY, sizeof(MedicineSupply), 0666);
    if (medicine_shmid == -1)
    {
        perror("shmget for medicine supply");
        return NULL;
    }

    // Attach shared memory for medicine supply
    MedicineSupply* medicine_supply = (MedicineSupply*)shmat(medicine_shmid, NULL, 0);
    if (medicine_supply == (MedicineSupply*)(-1))
    {
        perror("shmat for medicine supply");
        return NULL;
    }

    return medicine_supply;
}

void update_supplies_from_json(FoodSupply* food_supply, MedicineSupply* medicine_supply, cJSON* json)
{
    // Check if the JSON object contains the "food" field
    cJSON* food_object = cJSON_GetObjectItem(json, "food");
    if (food_object != NULL && food_object->type == cJSON_Object)
    {
        // Extract and update food supplies
        cJSON* fruits_item = cJSON_GetObjectItem(food_object, "fruits");
        if (fruits_item != NULL && fruits_item->type == cJSON_Number)
        {
            food_supply->fruits += fruits_item->valueint;
            if (food_supply->fruits < 0)
            {
                food_supply->fruits = 0;
            }
        }
        cJSON* vegetables_item = cJSON_GetObjectItem(food_object, "vegetables");
        if (vegetables_item != NULL && vegetables_item->type == cJSON_Number)
        {
            food_supply->vegetables += vegetables_item->valueint;
            if (food_supply->vegetables < 0)
            {
                food_supply->vegetables = 0;
            }
        }
        cJSON* meat_item = cJSON_GetObjectItem(food_object, "meat");
        if (meat_item != NULL && meat_item->type == cJSON_Number)
        {
            food_supply->meat += meat_item->valueint;
            if (food_supply->meat < 0)
            {
                food_supply->meat = 0;
            }
        }
        cJSON* water_item = cJSON_GetObjectItem(food_object, "water");
        if (water_item != NULL && water_item->type == cJSON_Number)
        {
            food_supply->water += water_item->valueint;
            if (food_supply->water < 0)
            {
                food_supply->water = 0;
            }
        }
    }

    // Check if the JSON object contains the "medicine" field
    cJSON* medicine_object = cJSON_GetObjectItem(json, "medicine");
    if (medicine_object != NULL && medicine_object->type == cJSON_Object)
    {
        // Extract and update medicine supplies
        cJSON* antibiotics_item = cJSON_GetObjectItem(medicine_object, "antibiotics");
        if (antibiotics_item != NULL && antibiotics_item->type == cJSON_Number)
        {
            medicine_supply->antibiotics += antibiotics_item->valueint;
            if (medicine_supply->antibiotics < 0)
            {
                medicine_supply->antibiotics = 0;
            }
        }
        cJSON* analgesics_item = cJSON_GetObjectItem(medicine_object, "analgesics");
        if (analgesics_item != NULL && analgesics_item->type == cJSON_Number)
        {
            medicine_supply->analgesics += analgesics_item->valueint;
            if (medicine_supply->analgesics < 0)
            {
                medicine_supply->analgesics = 0;
            }
        }
        cJSON* bandages_item = cJSON_GetObjectItem(medicine_object, "bandages");
        if (bandages_item != NULL && bandages_item->type == cJSON_Number)
        {
            medicine_supply->bandages += bandages_item->valueint;
            if (medicine_supply->bandages < 0)
            {
                medicine_supply->bandages = 0;
            }
        }
    }
}
