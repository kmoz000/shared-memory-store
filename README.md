# Bun Shared State

A high-performance C++ native addon for Node.js/Bun that provides a shared memory store with mutable keys.

## Features

- 🚀 High-performance in-memory key-value store
- 🔑 Mutable keys that update references across your application
- ⏱️ TTL support for automatic key expiration
- 🧹 Background cleanup of expired items
- 🔄 Thread-safe operations
- 🛡️ Error-resistant string conversion

## Installation

```bash
npm install bun-shared-state
# or
bun add bun-shared-state
```

## Basic Usage

```javascript
const MemoryStore = require('bun-shared-state');

// Create a memory store with cleanup every 5 seconds
const store = new MemoryStore({ cleanupInterval: 5000 });

// Start the cleanup task
store.startCleanupTask();

// Regular key-value operations
store.set('user:123', { name: 'Alice', role: 'admin' });
const user = store.get('user:123');
console.log(user); // { name: 'Alice', role: 'admin' }

// Set with TTL (Time-To-Live)
store.set('session:abc', { userId: 123, token: 'xyz' }, { 
  isPermanent: false, 
  maxAgeMs: 30000 // expire after 30 seconds
});

// Check if a key exists
if (store.has('user:123')) {
  console.log('User exists!');
}

// Delete a key
store.delete('user:123');

// Get all keys
const keys = store.keys();
console.log(keys); // ['session:abc', ...]

// Clear the store
store.clear();

// Get the number of items in the store
console.log(store.size()); // 0

// Stop the cleanup task before exit
store.stopCleanupTask();
```

## Advanced: Mutable Keys

One of the most powerful features is the ability to create mutable keys that can be used throughout your codebase and automatically update their string representation:

```javascript
// Create a mutable key
const userConfigKey = store.createKey('user-config');

// Store data with the key
store.set(userConfigKey, { theme: 'dark' });

// Pass the key around your application
function getUserTheme(store, key) {
  const config = store.get(key);
  return config?.theme;
}

console.log(getUserTheme(store, userConfigKey)); // 'dark'

// Change the key everywhere it's referenced
userConfigKey.value = 'user-settings';

// The same reference now points to a different storage location
store.set(userConfigKey, { theme: 'light' });
console.log(getUserTheme(store, userConfigKey)); // 'light'
```

## Architecture and Data Flow

### System Architecture

```
┌─────────────────┐        ┌───────────────────┐
│                 │        │                   │
│  JavaScript     │◄──────►│  C++ Native Addon │
│  Application    │        │  (MemoryStore)    │
│                 │        │                   │
└────────┬────────┘        └───────────┬───────┘
         │                             │
         │                             │
         ▼                             ▼
┌─────────────────┐        ┌───────────────────┐
│                 │        │                   │
│  Mutable Keys   │◄──────►│  Memory Store     │
│  (JS Proxies)   │        │  (C++ HashMap)    │
│                 │        │                   │
└─────────────────┘        └───────────────────┘
```

### Data Flow

1. **Set Operation**: When `store.set(key, value)` is called:
   - The key is converted to a string representation
   - If it's a mutable key, its current value is retrieved
   - Data is stored in the C++ `std::unordered_map`
   - JavaScript references are kept alive via N-API

2. **Get Operation**: When `store.get(key)` is called:
   - The key string is retrieved (or converted from the input)
   - The C++ addon looks up the value in the hash map
   - If found and not expired, the JS value is returned

3. **Mutable Key**: When a key value is changed via `key.value = newValue`:
   - The JS Proxy intercepts the change
   - It updates the target object in JavaScript
   - It notifies the C++ addon about the key change
   - Existing values remain accessible via the new key string

### Memory Management

```
┌─────────────────────────────────────────────────────────┐
│                    Node.js / Bun Process                │
│                                                         │
│  ┌───────────────┐          ┌────────────────────┐     │
│  │               │          │                    │     │
│  │  V8 JavaScript│          │  C++ Native Memory │     │
│  │  Heap         │          │  (MemoryStore)     │     │
│  │               │◄────────►│                    │     │
│  │  - JS Objects │          │  - Hash Tables     │     │
│  │  - Proxies    │          │  - References      │     │
│  │  - Functions  │          │  - Mutexes         │     │
│  │               │          │                    │     │
│  └───────────────┘          └────────────────────┘     │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### N-API Integration

The addon uses N-API (Node-API) to safely interact with the V8 engine:

- `Napi::ObjectWrap` - Wraps C++ object with JavaScript object
- `Napi::Reference` - Keeps JavaScript values alive for later use
- `Napi::ObjectReference` - Prevents JavaScript objects from being garbage collected
- `std::mutex` - Provides thread safety for operations in the C++ layer

## API Reference

### Constructor

#### `new MemoryStore([options])`

Creates a new memory store instance.

**Parameters:**
- `options` (Object, optional)
  - `cleanupInterval` (Number): Milliseconds between cleanup operations (default: 60000)

### Methods

#### `store.set(key, value, [options])`

Sets a value in the store.

**Parameters:**
- `key`: String, object, or mutable key
- `value`: Any JavaScript value
- `options` (Object, optional)
  - `isPermanent` (Boolean): If false, the item can expire (default: true)
  - `maxAgeMs` (Number): Time in milliseconds before the item expires (default: 0)

**Returns:** Boolean

#### `store.get(key)`

Gets a value from the store.

**Parameters:**
- `key`: String, object, or mutable key

**Returns:** The stored value or `undefined` if not found

#### `store.has(key)`

Checks if a key exists in the store and hasn't expired.

**Parameters:**
- `key`: String, object, or mutable key

**Returns:** Boolean

#### `store.delete(key)`

Removes a key from the store.

**Parameters:**
- `key`: String, object, or mutable key

**Returns:** Boolean (true if key was found and deleted)

#### `store.clear()`

Removes all keys from the store.

**Returns:** Boolean

#### `store.size()`

Gets the number of items in the store.

**Returns:** Number

#### `store.keys()`

Gets all keys in the store as strings.

**Returns:** Array of strings

#### `store.getKeys()`

Gets all keys in the store in their original form.

**Returns:** Array of keys

#### `store.createKey(initialValue)`

Creates a mutable key.

**Parameters:**
- `initialValue`: Initial string or object to use as the key

**Returns:** Proxy object with a mutable value property

#### `store.startCleanupTask([intervalMs])`

Starts the background cleanup task.

**Parameters:**
- `intervalMs` (Number, optional): Override the cleanup interval

**Returns:** Boolean

#### `store.stopCleanupTask()`

Stops the background cleanup task.

**Returns:** Boolean

## Performance Considerations

- The memory store uses a C++ `std::unordered_map` which provides O(1) average case lookup
- String conversions are optimized and cached when possible
- For best performance, use string keys directly rather than complex objects
- Mutable keys add flexibility but have slightly more overhead than static strings
- TTL (time-to-live) cleanup is handled in a background thread to avoid blocking the main thread

## Building from Source

Requirements:
- Node.js 14+ or Bun
- C++17 compatible compiler
- node-gyp

```bash
git clone https://github.com/yourusername/bun-shared-state.git
cd bun-shared-state
npm install
npm run build
```

## License

MIT
