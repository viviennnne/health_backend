/**
 * Health Tracker Backend Test Script
 * * Usage: node health_tracker_test.js
 * * This script sequentially tests the Authentication, Profile, Water, Sleep,
 * Activity, and Custom Category endpoints based on the provided JSON API spec.
 */

const BASE_URL = "http://localhost:8080"; // Change this to your server address
let JWT_TOKEN = null;
let USER_ID = null;

// ANSI Colors for Console Output
const COLORS = {
  reset: "\x1b[0m",
  green: "\x1b[32m",
  red: "\x1b[31m",
  yellow: "\x1b[33m",
  blue: "\x1b[34m",
  cyan: "\x1b[36m",
  gray: "\x1b[90m",
};

// --- Helper Functions ---

/**
 * proper logging helper
 */
function log(step, message, status = "INFO") {
  let color = COLORS.reset;
  if (status === "PASS") color = COLORS.green;
  else if (status === "FAIL") color = COLORS.red;
  else if (status === "WARN") color = COLORS.yellow;
  else if (status === "SECTION") color = COLORS.cyan;
  else if (status === "TIME") color = COLORS.gray;

  console.log(`${color}[${status}] ${step}: ${message}${COLORS.reset}`);
}

/**
 * Generic API Request Wrapper
 */
async function apiRequest(endpoint, method = "GET", body = null) {
  const headers = {
    "Content-Type": "application/json",
  };

  if (JWT_TOKEN) {
    headers["Authorization"] = `Bearer ${JWT_TOKEN}`;
  }

  const options = {
    method,
    headers,
  };

  if (body) {
    options.body = JSON.stringify(body);
  }

  const startTime = Date.now();

  try {
    const response = await fetch(`${BASE_URL}${endpoint}`, options);
    const duration = Date.now() - startTime;

    // Log the request timing
    log(
      "TIME",
      `${method} ${endpoint} (${response.status}) took ${duration}ms`,
      "TIME",
    );

    let responseData = null;
    const contentType = response.headers.get("content-type");

    // Only try to parse JSON if content-type says so, or if it's not a 204 (No Content)
    if (
      response.status !== 204 &&
      contentType &&
      contentType.includes("application/json")
    ) {
      responseData = await response.json();
    }

    if (!response.ok) {
      return {
        success: false,
        status: response.status,
        duration,
        error: responseData || response.statusText,
      };
    }

    return {
      success: true,
      status: response.status,
      duration,
      data: responseData,
    };
  } catch (err) {
    const duration = Date.now() - startTime;
    log(
      "NETWORK",
      `Failed to connect to ${BASE_URL}${endpoint} after ${duration}ms`,
      "FAIL",
    );
    console.error(err);
    process.exit(1);
  }
}

// --- Test Suites ---

async function testAuthentication() {
  log("AUTH", "Starting Authentication Tests...", "SECTION");

  // 1. Register
  const uniqueName = `user_${Date.now()}`;
  const userPayload = {
    name: uniqueName,
    password: "securePassword123",
    age: 30,
    weightKg: 75,
    heightM: 1.8,
    gender: "male",
  };

  const registerRes = await apiRequest("/register", "POST", userPayload);

  if (registerRes.success && registerRes.data.token) {
    JWT_TOKEN = registerRes.data.token;
    log("Register", `User registered: ${uniqueName}`, "PASS");
  } else {
    log("Register", `Failed: ${JSON.stringify(registerRes.error)}`, "FAIL");
    return false;
  }

  // 2. Login (Optional verification since register returns token, but good to test)
  const loginPayload = { name: uniqueName, password: "securePassword123" };
  const loginRes = await apiRequest("/login", "POST", loginPayload);

  if (loginRes.success && loginRes.data.token) {
    // Update token just in case
    JWT_TOKEN = loginRes.data.token;
    log("Login", "User logged in successfully", "PASS");
  } else {
    log("Login", `Failed to login: ${JSON.stringify(loginRes.error)}`, "FAIL");
  }

  return true;
}

async function testProfile() {
  log("PROFILE", "Testing Profile Endpoints...", "SECTION");

  // 1. Get Profile
  const profileRes = await apiRequest("/user/profile", "GET");
  if (profileRes.success && profileRes.data.name) {
    USER_ID = profileRes.data.id;
    log("Profile", `Fetched profile for ID: ${USER_ID}`, "PASS");
  } else {
    log(
      "Profile",
      `Failed to fetch profile: ${JSON.stringify(profileRes.error)}`,
      "FAIL",
    );
  }

  // 2. Get BMI
  const bmiRes = await apiRequest("/user/bmi", "GET");
  if (bmiRes.success && bmiRes.data.bmi) {
    log("BMI", `Calculated BMI: ${bmiRes.data.bmi}`, "PASS");
  } else {
    log("BMI", `Failed to fetch BMI: ${JSON.stringify(bmiRes.error)}`, "FAIL");
  }
}

async function testStandardResource(
  resourceName,
  endpoint,
  createPayload,
  updatePayload,
) {
  log(resourceName.toUpperCase(), `Testing ${resourceName} CRUD...`, "SECTION");
  let createdId = null;

  // 1. Create (POST)
  const createRes = await apiRequest(endpoint, "POST", createPayload);
  if (createRes.success && createRes.data.id) {
    createdId = createRes.data.id;
    log(`${resourceName} Create`, `Created item ID: ${createdId}`, "PASS");
  } else {
    log(
      `${resourceName} Create`,
      `Failed: ${JSON.stringify(createRes.error)}`,
      "FAIL",
    );
    return; // Stop if create fails
  }

  // 2. Read (GET List)
  const getRes = await apiRequest(endpoint, "GET");
  if (getRes.success && Array.isArray(getRes.data)) {
    const found = getRes.data.find((item) => item.id === createdId);
    if (found) {
      log(`${resourceName} Read`, `Item found in list`, "PASS");
    } else {
      log(
        `${resourceName} Read`,
        `Item ID ${createdId} not found in list`,
        "FAIL",
      );
    }
  } else {
    log(
      `${resourceName} Read`,
      `Failed to fetch list: ${JSON.stringify(getRes.error)}`,
      "FAIL",
    );
  }

  // 3. Update (PATCH)
  const patchRes = await apiRequest(
    `${endpoint}/${createdId}`,
    "PATCH",
    updatePayload,
  );
  if (patchRes.success) {
    // Verify update logic (simple check)
    const keys = Object.keys(updatePayload);
    const check = keys.every(
      (key) => patchRes.data[key] === updatePayload[key],
    );
    if (check) {
      log(`${resourceName} Update`, `Item updated successfully`, "PASS");
    } else {
      log(`${resourceName} Update`, `Response did not match payload`, "WARN");
    }
  } else {
    log(
      `${resourceName} Update`,
      `Failed: ${JSON.stringify(patchRes.error)}`,
      "FAIL",
    );
  }

  // 4. Delete (DELETE)
  const deleteRes = await apiRequest(`${endpoint}/${createdId}`, "DELETE",{});
  if (deleteRes.success && deleteRes.status === 204) {
    log(`${resourceName} Delete`, `Item deleted successfully`, "PASS");
  } else {
    log(
      `${resourceName} Delete`,
      `Failed: ${JSON.stringify(deleteRes.error)}`,
      "FAIL",
    );
  }
}

async function testCustomCategories() {
  log("CATEGORIES", "Testing Custom Categories & Items...", "SECTION");

  let catId = null;
  let itemId = null;

  // 1. Create Category
  const catPayload = { categoryName: "Mindfulness" };
  const catRes = await apiRequest("/category/create", "POST", catPayload);

  if (catRes.success && catRes.data.id) {
    catId = catRes.data.id;
    log(
      "Category Create",
      `Created Category: ${catRes.data.categoryName} (${catId})`,
      "PASS",
    );
  } else {
    log(
      "Category Create",
      `Failed to create category: ${JSON.stringify(catRes.error)}`,
      "FAIL",
    );
    return;
  }

  // 2. List Categories
  const listRes = await apiRequest("/category/list", "GET");
  if (listRes.success && listRes.data.some((c) => c.id === catId)) {
    log("Category List", "New category found in list", "PASS");
  } else {
    log(
      "Category List",
      `Category not found or request failed: ${JSON.stringify(listRes.error || "N/A")}`,
      "FAIL",
    );
  }

  // 3. Add Item to Category
  const itemPayload = {
    datetime: new Date().toISOString(),
    note: "Morning Meditation",
  };
  const addItemRes = await apiRequest(
    `/category/${catId}/add`,
    "POST",
    itemPayload,
  );

  if (addItemRes.success && addItemRes.data.id) {
    itemId = addItemRes.data.id;
    log("Category Item Add", `Added Item ID: ${itemId}`, "PASS");
  } else {
    log(
      "Category Item Add",
      `Failed to add item: ${JSON.stringify(addItemRes.error)}`,
      "FAIL",
    );
    return;
  }

  // 4. List Items in Category
  const listItemRes = await apiRequest(`/category/${catId}/list`, "GET");
  if (listItemRes.success && listItemRes.data.some((i) => i.id === itemId)) {
    log("Category Item List", "Item found in category list", "PASS");
  } else {
    log(
      "Category Item List",
      `Item not found or request failed: ${JSON.stringify(listItemRes.error || "N/A")}`,
      "FAIL",
    );
  }

  // 5. Update Item
  const updateItemPayload = { note: "Evening Meditation" };
  const patchItemRes = await apiRequest(
    `/category/${catId}/${itemId}`,
    "PATCH",
    updateItemPayload,
  );
  if (patchItemRes.success && patchItemRes.data.note === "Evening Meditation") {
    log("Category Item Update", "Item updated successfully", "PASS");
  } else {
    log(
      "Category Item Update",
      `Failed to update item: ${JSON.stringify(patchItemRes.error)}`,
      "FAIL",
    );
  }

  // 6. Delete Item
  const delItemRes = await apiRequest(`/category/${catId}/${itemId}`, "DELETE",{});
  if (delItemRes.success) {
    log("Category Item Delete", "Item deleted successfully", "PASS");
  } else {
    log(
      "Category Item Delete",
      `Failed to delete item: ${JSON.stringify(delItemRes.error)}`,
      "FAIL",
    );
  }
}

// --- Main Execution Flow ---

async function runTests() {
  console.log(`\nStarting API Tests against ${BASE_URL}...\n`);

  const authSuccess = await testAuthentication();
  if (!authSuccess) {
    log("CRITICAL", "Authentication failed. Aborting tests.", "FAIL");
    process.exit(1);
  }

  await testProfile();

  // Test Waters
  await testStandardResource(
    "Waters",
    "/waters",
    { datetime: new Date().toISOString(), amountMl: 500 },
    { amountMl: 750 },
  );

  // Test Sleeps
  await testStandardResource(
    "Sleeps",
    "/sleeps",
    { datetime: new Date().toISOString(), hours: 6.5 },
    { hours: 8.0 },
  );

  // Test Activities
  await testStandardResource(
    "Activities",
    "/activities",
    { datetime: new Date().toISOString(), minutes: 30, intensity: "moderate" },
    { minutes: 45, intensity: "high" },
  );

  // Test Custom Categories
  await testCustomCategories();

  console.log(`\n${COLORS.green}All tests completed.${COLORS.reset}\n`);
}

// Run
runTests();
