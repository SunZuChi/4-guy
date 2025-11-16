function createBankAccount(initialBalance = 0) {
  let balance = Number(initialBalance) || 0;

  function getBalance() {
    return balance;
  }

  function deposit(amount) {
    amount = Number(amount);
    if (!isFinite(amount) || amount <= 0) {
      console.log("Deposit amount must be a positive number.");
      return;
    }
    balance += amount;
    return balance;
  }

  function withdraw(amount) {
    amount = Number(amount);
    if (!isFinite(amount) || amount <= 0) {
      console.log("Withdraw amount must be a positive number.");
      return;
    }
    if (amount > balance) {
      console.log("Withdrawal failed: insufficient funds.");
      return;
    }
    balance -= amount;
    return balance;
  }

  return {
    getBalance,
    deposit,
    withdraw
  };
}

const acc = createBankAccount(100);
console.log(acc.getBalance());
acc.deposit(50);
console.log(acc.getBalance());
acc.withdraw(30);
console.log(acc.getBalance()); 
acc.withdraw(200);
console.log(acc.getBalance()); 