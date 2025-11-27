import java.util.*;
class User{
    String name;
    protected double balance;
    String email;
    //Constructor
    User(String name, double balance, String email){
        this.name = name;
        this.balance = balance;
        this.email = email;
    }

    //Display
    void display_detail(){
       System.out.println("Name: "      + name );
       System.out.println("Balance: "   + balance);
       System.out.println("Email: "     + email);
    }

    //Add money
    protected void add_money(int money){
        balance += money;  
    }
    void show_menu(Market market, Scanner in){
        System.out.println("Nothing");
    }
}
