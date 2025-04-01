const { MemoryStore } = require('./build/Release/memorystore');

class MemoryStoreWrapper {
  constructor(options = {}) {
    this._store = new MemoryStore(options);
    
    // Start cleanup task if not specified otherwise
    if (options.autoStartCleanup !== false) {
      this._store.startCleanupTask(options.cleanupInterval || 60000);
    }
  }

  /**
   * Create a mutable key that will update all references when changed
   * @param {any} initialValue - The initial value for the key
   * @returns {Proxy} - A proxy object that can be used as a mutable key
   */
  createKey(initialValue) {
    return this._store.createMutableKey(initialValue);
  }

  /**
   * Store a value in memory
   * @param {string|Proxy} key - The key to store the value under (can be a string or mutable key)
   * @param {any} value - The value to store (can be any JavaScript value)
   * @param {Object} options - Storage options
   * @param {boolean} options.isPermanent - If true, item never expires (default: true)
   * @param {number} options.maxAgeMs - Time in ms before item expires (only if isPermanent is false)
   * @returns {boolean} - Success status
   */
  set(key, value, options = { isPermanent: true }) {
    return this._store.set(key, value, options);
  }

  /**
   * Retrieve a value from memory
   * @param {string|Proxy} key - The key to retrieve
   * @returns {any} - The stored value or undefined if not found or expired
   */
  get(key) {
    return this._store.get(key);
  }

  /**
   * Check if a key exists and is not expired
   * @param {string|Proxy} key - The key to check
   * @returns {boolean} - True if key exists and not expired
   */
  has(key) {
    return this._store.has(key);
  }

  /**
   * Delete a value from memory
   * @param {string|Proxy} key - The key to delete
   * @returns {boolean} - True if key was found and deleted
   */
  delete(key) {
    return this._store.delete(key);
  }

  /**
   * Clear all stored values
   * @returns {boolean} - Success status
   */
  clear() {
    return this._store.clear();
  }

  /**
   * Get the number of items in the store
   * @returns {number} - Count of items
   */
  size() {
    return this._store.size();
  }

  /**
   * Get all keys in the store as strings
   * @returns {string[]} - Array of key strings
   */
  keys() {
    return this._store.keys();
  }

  /**
   * Get all keys in the store (including original key objects)
   * @returns {Array} - Array of original key references (strings or mutable keys)
   */
  getKeys() {
    return this._store.getKeys();
  }

  /**
   * Start the cleanup task for expired items
   * @param {number} intervalMs - Cleanup interval in milliseconds
   * @returns {boolean} - Success status
   */
  startCleanupTask(intervalMs = 60000) {
    return this._store.startCleanupTask(intervalMs);
  }

  /**
   * Stop the cleanup task
   * @returns {boolean} - Success status
   */
  stopCleanupTask() {
    return this._store.stopCleanupTask();
  }
}

module.exports = MemoryStoreWrapper;