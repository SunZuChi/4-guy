function createBankAccount (initialBalance){
    
    let balance = initialBalance;

    return {
     getBalance : function()  {return balance},
     deposit : function(amount) {return balance + amount},
     withdraw : function(amount) {
        if (balance < amount) return "Sorry you don't have enough money to withdraw";
        else {return balance - amount};
        }
    }
    
}

let client1 = new createBankAccount(50000);
client1.getBalance();