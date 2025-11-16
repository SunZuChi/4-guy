function createItemManager() {
  const items = new Set();

  function addItem(item) {
    if (typeof item !== "string") {
      console.log("Item must be a string.");
      return false;
    }
    if (items.has(item)) {
      console.log(`"${item}" is already in the list.`);
      return false;
    }
    items.add(item);
    return true;
  }

  function removeItem(item) {
    if (!items.has(item)) {
      console.log(`"${item}" not found.`);
      return false;
    }
    items.delete(item);
    return true;
  }

  function listItems() {
    return Array.from(items);
  }

  return {
    addItem,
    removeItem,
    listItems
  };
}

const mgr = createItemManager();
mgr.addItem("apple"); 
mgr.addItem("banana"); 
mgr.addItem("apple");
console.log(mgr.listItems()); 
mgr.removeItem("orange");
mgr.removeItem("banana"); 
console.log(mgr.listItems());