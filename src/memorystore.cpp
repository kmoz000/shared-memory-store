#include <napi.h>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <memory>

class MemoryStore : public Napi::ObjectWrap<MemoryStore> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  MemoryStore(const Napi::CallbackInfo& info);
  ~MemoryStore();

private:
  struct StoreItem {
    Napi::Reference<Napi::Value> value;
    Napi::Reference<Napi::Value> keyRef; // Store reference to the key
    bool isPermanent;
    std::chrono::steady_clock::time_point expiresAt;
    uint64_t maxAgeMs;
  };

  // Custom key wrapper for proxy monitoring
  struct KeyWrapper {
    std::string keyString;
    Napi::ObjectReference proxyRef;
  };

  // Safe string conversion helper
  std::string SafeGetString(const Napi::Value& value) {
    if (value.IsNull() || value.IsUndefined()) {
      return "";
    }

    if (value.IsString()) {
      return value.As<Napi::String>().Utf8Value();
    }

    Napi::Env env = value.Env();
    try {
      if (value.IsObject()) {
        Napi::Object obj = value.As<Napi::Object>();
        if (obj.Has("toString") && obj.Get("toString").IsFunction()) {
          Napi::Function toStringFn = obj.Get("toString").As<Napi::Function>();
          Napi::Value strValue = toStringFn.Call(obj, {});
          if (strValue.IsString()) {
            return strValue.As<Napi::String>().Utf8Value();
          }
        }
      }
      // Fallback to default conversion
      return value.ToString().Utf8Value();
    } catch (const Napi::Error& e) {
      // Log the error and return an empty string
      Napi::Error::Fatal("SafeGetString", ("Failed to convert value to string: " + std::string(e.Message())).c_str());
      return "";
    }
  }

  Napi::Value Set(const Napi::CallbackInfo& info);
  Napi::Value Get(const Napi::CallbackInfo& info);
  Napi::Value Has(const Napi::CallbackInfo& info);
  Napi::Value Delete(const Napi::CallbackInfo& info);
  Napi::Value Clear(const Napi::CallbackInfo& info);
  Napi::Value Size(const Napi::CallbackInfo& info);
  Napi::Value Keys(const Napi::CallbackInfo& info);
  Napi::Value GetKeys(const Napi::CallbackInfo& info);
  Napi::Value StartCleanupTask(const Napi::CallbackInfo& info);
  Napi::Value StopCleanupTask(const Napi::CallbackInfo& info);
  Napi::Value CreateMutableKey(const Napi::CallbackInfo& info);

  void CleanupExpiredItems();
  void CleanupWorker();

  std::unordered_map<std::string, StoreItem> store;
  std::unordered_map<std::string, std::shared_ptr<KeyWrapper>> keyWrappers;
  std::mutex storeMutex;
  std::thread cleanupThread;
  std::condition_variable cleanupCV;
  std::atomic<bool> stopCleanup;
  uint64_t cleanupIntervalMs;
};

Napi::Object MemoryStore::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "MemoryStore", {
    InstanceMethod("set", &MemoryStore::Set),
    InstanceMethod("get", &MemoryStore::Get),
    InstanceMethod("has", &MemoryStore::Has),
    InstanceMethod("delete", &MemoryStore::Delete),
    InstanceMethod("clear", &MemoryStore::Clear),
    InstanceMethod("size", &MemoryStore::Size),
    InstanceMethod("keys", &MemoryStore::Keys),
    InstanceMethod("getKeys", &MemoryStore::GetKeys),
    InstanceMethod("startCleanupTask", &MemoryStore::StartCleanupTask),
    InstanceMethod("stopCleanupTask", &MemoryStore::StopCleanupTask),
    InstanceMethod("createMutableKey", &MemoryStore::CreateMutableKey)
  });

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData(constructor);

  exports.Set("MemoryStore", func);
  return exports;
}

MemoryStore::MemoryStore(const Napi::CallbackInfo& info) 
  : Napi::ObjectWrap<MemoryStore>(info), stopCleanup(true), cleanupIntervalMs(60000) {
  Napi::Env env = info.Env();

  if (info.Length() > 0 && info[0].IsObject()) {
    Napi::Object options = info[0].As<Napi::Object>();
    
    if (options.Has("cleanupInterval") && options.Get("cleanupInterval").IsNumber()) {
      cleanupIntervalMs = options.Get("cleanupInterval").As<Napi::Number>().Uint32Value();
    }
  }
}

MemoryStore::~MemoryStore() {
  stopCleanup = true;
  cleanupCV.notify_one();
  
  if (cleanupThread.joinable()) {
    cleanupThread.join();
  }
}

Napi::Value MemoryStore::CreateMutableKey(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Initial value required").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value initialValue = info[0];
  std::string initialKeyString = SafeGetString(initialValue);
  
  // Create unique ID for this key
  std::string uniqueId = initialKeyString + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

  // Create the key wrapper
  auto keyWrapper = std::make_shared<KeyWrapper>();
  keyWrapper->keyString = initialKeyString;

  // Store in our map (thread-safe)
  {
    std::lock_guard<std::mutex> lock(storeMutex);
    keyWrappers[uniqueId] = keyWrapper;
  }

  // Create the Proxy object in JavaScript
  Napi::Object global = env.Global();
  Napi::Object proxyConstructor = global.Get("Proxy").As<Napi::Object>();
  
  // Create the target object
  Napi::Object target = Napi::Object::New(env);
  target.Set("value", initialValue);
  target.Set("__keyId", Napi::String::New(env, uniqueId));
  
  // Create the handler object
  Napi::Object handler = Napi::Object::New(env);
  
  // Set handler
  Napi::Function getFunction = Napi::Function::New(env, [](const Napi::CallbackInfo& info) -> Napi::Value {
    Napi::Env env = info.Env();
    Napi::Object target = info[0].As<Napi::Object>();
    Napi::Value prop = info[1];
    
    if (prop.IsString() && prop.As<Napi::String>().Utf8Value() == "toString") {
      return Napi::Function::New(env, [](const Napi::CallbackInfo& info) -> Napi::Value {
        Napi::Env env = info.Env();
        Napi::Object thisObj = info.This().As<Napi::Object>();
        Napi::Object target = thisObj.Get("target").As<Napi::Object>();
        return target.Get("value").ToString();
      }, "toString", target);
    }

    if (prop.IsString() && prop.As<Napi::String>().Utf8Value() == "valueOf") {
      return Napi::Function::New(env, [](const Napi::CallbackInfo& info) -> Napi::Value {
        Napi::Env env = info.Env();
        Napi::Object thisObj = info.This().As<Napi::Object>();
        Napi::Object target = thisObj.Get("target").As<Napi::Object>();
        return target.Get("value");
      }, "valueOf", target);
    }
    
    return target.Get("value");
  });
  
  Napi::Function setFunction = Napi::Function::New(env, [this](const Napi::CallbackInfo& info) -> Napi::Value {
    Napi::Env env = info.Env();
    Napi::Object target = info[0].As<Napi::Object>();
    Napi::Value newValue = info[2];
    
    // Get the keyId to identify which key is being modified
    std::string keyId = target.Get("__keyId").As<Napi::String>().Utf8Value();
    
    std::string newKeyString = SafeGetString(newValue);
    
    // Update the target
    target.Set("value", newValue);
    
    // Update our key wrapper (thread-safe)
    {
      std::lock_guard<std::mutex> lock(storeMutex);
      auto it = keyWrappers.find(keyId);
      if (it != keyWrappers.end()) {
        it->second->keyString = newKeyString;
      }
    }
    
    return Napi::Boolean::New(env, true);
  });
  
  handler.Set("get", getFunction);
  handler.Set("set", setFunction);
  
  // Create the proxy object
  Napi::Function proxyFunction = proxyConstructor.As<Napi::Function>();
  Napi::Object proxyObject = proxyFunction.New({ target, handler });
  
  // Keep a reference to the proxy
  keyWrapper->proxyRef = Napi::Persistent(proxyObject);
  
  return proxyObject;
}

Napi::Value MemoryStore::Set(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2) {
    Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value keyValue = info[0];
  Napi::Value value = info[1];
  
  bool isPermanent = true;
  uint64_t maxAgeMs = 0;

  if (info.Length() >= 3 && info[2].IsObject()) {
    Napi::Object options = info[2].As<Napi::Object>();
    
    if (options.Has("isPermanent") && options.Get("isPermanent").IsBoolean()) {
      isPermanent = options.Get("isPermanent").As<Napi::Boolean>().Value();
    }
    
    if (options.Has("maxAgeMs") && options.Get("maxAgeMs").IsNumber()) {
      maxAgeMs = options.Get("maxAgeMs").As<Napi::Number>().Uint32Value();
    }
  }

  // Determine if this is a mutable key proxy
  std::string keyString;
  std::string keyId;
  
  if (!keyValue.IsNull() && !keyValue.IsUndefined() && keyValue.IsObject() && !keyValue.IsString()) {
    Napi::Object keyObj = keyValue.As<Napi::Object>();
    
    // Check if it has our special property
    if (keyObj.Has("__keyId")) {
      Napi::Value idValue = keyObj.Get("__keyId");
      if (!idValue.IsNull() && !idValue.IsUndefined() && idValue.IsString()) {
        keyId = idValue.As<Napi::String>().Utf8Value();
        
        // Look up the current key string
        std::lock_guard<std::mutex> lock(storeMutex);
        auto it = keyWrappers.find(keyId);
        if (it != keyWrappers.end()) {
          keyString = it->second->keyString;
        } else {
          // If not found, fall back to string representation
          keyString = SafeGetString(keyValue);
        }
      } else {
        keyString = SafeGetString(keyValue);
      }
    } else {
      // Regular object, use toString
      keyString = SafeGetString(keyValue);
    }
  } else {
    // Regular key (string or other primitive)
    keyString = SafeGetString(keyValue);
  }

  StoreItem item;
  item.value = Napi::Persistent(value);
  item.keyRef = Napi::Persistent(keyValue); // Store reference to original key object
  item.isPermanent = isPermanent;
  item.maxAgeMs = maxAgeMs;
  
  if (!isPermanent && maxAgeMs > 0) {
    item.expiresAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxAgeMs);
  } else {
    item.expiresAt = std::chrono::steady_clock::time_point::max();
  }

  {
    std::lock_guard<std::mutex> lock(storeMutex);
    store.insert_or_assign(keyString, std::move(item));
  }

  return Napi::Boolean::New(env, true);
}

Napi::Value MemoryStore::Get(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Key is required").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value keyValue = info[0];
  
  // Similar logic as in Set to determine the key string
  std::string keyString;
  std::string keyId;
  
  if (!keyValue.IsNull() && !keyValue.IsUndefined() && keyValue.IsObject() && !keyValue.IsString()) {
    Napi::Object keyObj = keyValue.As<Napi::Object>();
    
    if (keyObj.Has("__keyId")) {
      Napi::Value idValue = keyObj.Get("__keyId");
      if (!idValue.IsNull() && !idValue.IsUndefined() && idValue.IsString()) {
        keyId = idValue.As<Napi::String>().Utf8Value();
        
        std::lock_guard<std::mutex> lock(storeMutex);
        auto it = keyWrappers.find(keyId);
        if (it != keyWrappers.end()) {
          keyString = it->second->keyString;
        } else {
          keyString = SafeGetString(keyValue);
        }
      } else {
        keyString = SafeGetString(keyValue);
      }
    } else {
      keyString = SafeGetString(keyValue);
    }
  } else {
    keyString = SafeGetString(keyValue);
  }
  
  std::lock_guard<std::mutex> lock(storeMutex);
  auto it = store.find(keyString);
  
  if (it != store.end()) {
    // Check if item is expired
    if (!it->second.isPermanent && it->second.maxAgeMs > 0) {
      auto now = std::chrono::steady_clock::now();
      if (now >= it->second.expiresAt) {
        store.erase(it);
        return env.Undefined();
      }
    }
    return it->second.value.Value();
  }
  
  return env.Undefined();
}

Napi::Value MemoryStore::Has(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Key is required").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value keyValue = info[0];
  
  // Similar logic as in Get to determine the key string
  std::string keyString;
  std::string keyId;
  
  if (!keyValue.IsNull() && !keyValue.IsUndefined() && keyValue.IsObject() && !keyValue.IsString()) {
    Napi::Object keyObj = keyValue.As<Napi::Object>();
    
    if (keyObj.Has("__keyId")) {
      Napi::Value idValue = keyObj.Get("__keyId");
      if (!idValue.IsNull() && !idValue.IsUndefined() && idValue.IsString()) {
        keyId = idValue.As<Napi::String>().Utf8Value();
        
        std::lock_guard<std::mutex> lock(storeMutex);
        auto it = keyWrappers.find(keyId);
        if (it != keyWrappers.end()) {
          keyString = it->second->keyString;
        } else {
          keyString = SafeGetString(keyValue);
        }
      } else {
        keyString = SafeGetString(keyValue);
      }
    } else {
      keyString = SafeGetString(keyValue);
    }
  } else {
    keyString = SafeGetString(keyValue);
  }
  
  std::lock_guard<std::mutex> lock(storeMutex);
  auto it = store.find(keyString);
  
  if (it != store.end()) {
    // Check if item is expired
    if (!it->second.isPermanent && it->second.maxAgeMs > 0) {
      auto now = std::chrono::steady_clock::now();
      if (now >= it->second.expiresAt) {
        store.erase(it);
        return Napi::Boolean::New(env, false);
      }
    }
    return Napi::Boolean::New(env, true);
  }
  
  return Napi::Boolean::New(env, false);
}

Napi::Value MemoryStore::Delete(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Key is required").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value keyValue = info[0];
  
  // Similar logic as in Get/Has to determine the key string
  std::string keyString;
  std::string keyId;
  
  if (!keyValue.IsNull() && !keyValue.IsUndefined() && keyValue.IsObject() && !keyValue.IsString()) {
    Napi::Object keyObj = keyValue.As<Napi::Object>();
    
    if (keyObj.Has("__keyId")) {
      Napi::Value idValue = keyObj.Get("__keyId");
      if (!idValue.IsNull() && !idValue.IsUndefined() && idValue.IsString()) {
        keyId = idValue.As<Napi::String>().Utf8Value();
        
        std::lock_guard<std::mutex> lock(storeMutex);
        auto it = keyWrappers.find(keyId);
        if (it != keyWrappers.end()) {
          keyString = it->second->keyString;
        } else {
          keyString = SafeGetString(keyValue);
        }
      } else {
        keyString = SafeGetString(keyValue);
      }
    } else {
      keyString = SafeGetString(keyValue);
    }
  } else {
    keyString = SafeGetString(keyValue);
  }
  
  std::lock_guard<std::mutex> lock(storeMutex);
  auto it = store.find(keyString);
  
  if (it != store.end()) {
    store.erase(it);
    return Napi::Boolean::New(env, true);
  }
  
  return Napi::Boolean::New(env, false);
}

Napi::Value MemoryStore::Clear(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  std::lock_guard<std::mutex> lock(storeMutex);
  store.clear();
  
  return Napi::Boolean::New(env, true);
}

Napi::Value MemoryStore::Size(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  std::lock_guard<std::mutex> lock(storeMutex);
  return Napi::Number::New(env, static_cast<uint32_t>(store.size()));
}

Napi::Value MemoryStore::Keys(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  std::vector<std::string> validKeys;
  auto now = std::chrono::steady_clock::now();
  
  {
    std::lock_guard<std::mutex> lock(storeMutex);
    for (const auto& pair : store) {
      // Check if item is not expired
      if (pair.second.isPermanent || pair.second.maxAgeMs == 0 || now < pair.second.expiresAt) {
        validKeys.push_back(pair.first);
      }
    }
  }
  
  Napi::Array keysArray = Napi::Array::New(env, validKeys.size());
  for (size_t i = 0; i < validKeys.size(); i++) {
    keysArray.Set(i, Napi::String::New(env, validKeys[i]));
  }
  
  return keysArray;
}

Napi::Value MemoryStore::GetKeys(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  auto now = std::chrono::steady_clock::now();
  
  // First count valid keys to pre-size the array
  size_t validKeyCount = 0;
  {
    std::lock_guard<std::mutex> lock(storeMutex);
    for (const auto& pair : store) {
      if (pair.second.isPermanent || pair.second.maxAgeMs == 0 || now < pair.second.expiresAt) {
        validKeyCount++;
      }
    }
  }
  
  Napi::Array keysArray = Napi::Array::New(env, validKeyCount);
  
  // Now populate the array directly, without using an intermediate vector
  {
    std::lock_guard<std::mutex> lock(storeMutex);
    size_t index = 0;
    for (const auto& pair : store) {
      if (pair.second.isPermanent || pair.second.maxAgeMs == 0 || now < pair.second.expiresAt) {
        keysArray.Set(index++, pair.second.keyRef.Value());
      }
    }
  }
  
  return keysArray;
}

Napi::Value MemoryStore::StartCleanupTask(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() > 0 && info[0].IsNumber()) {
    cleanupIntervalMs = info[0].As<Napi::Number>().Uint32Value();
  }
  
  if (!stopCleanup) {
    return Napi::Boolean::New(env, false); // Already running
  }
  
  stopCleanup = false;
  
  if (cleanupThread.joinable()) {
    cleanupThread.join();
  }
  
  cleanupThread = std::thread(&MemoryStore::CleanupWorker, this);
  
  return Napi::Boolean::New(env, true);
}

Napi::Value MemoryStore::StopCleanupTask(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (stopCleanup) {
    return Napi::Boolean::New(env, false); // Already stopped
  }
  
  stopCleanup = true;
  cleanupCV.notify_one();
  
  if (cleanupThread.joinable()) {
    cleanupThread.join();
  }
  
  return Napi::Boolean::New(env, true);
}

void MemoryStore::CleanupExpiredItems() {
  auto now = std::chrono::steady_clock::now();
  
  std::lock_guard<std::mutex> lock(storeMutex);
  for (auto it = store.begin(); it != store.end();) {
    if (!it->second.isPermanent && it->second.maxAgeMs > 0 && now >= it->second.expiresAt) {
      it = store.erase(it);
    } else {
      ++it;
    }
  }
}

void MemoryStore::CleanupWorker() {
  while (!stopCleanup) {
    CleanupExpiredItems();
    
    std::unique_lock<std::mutex> lock(storeMutex);
    cleanupCV.wait_for(lock, std::chrono::milliseconds(cleanupIntervalMs), [this] { return stopCleanup.load(); });
  }
}

Napi::Object InitModule(Napi::Env env, Napi::Object exports) {
  return MemoryStore::Init(env, exports);
}

NODE_API_MODULE(memorystore, InitModule)