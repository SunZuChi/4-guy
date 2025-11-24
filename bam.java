import java.util.ArrayList;
import java.util.List;
import java.util.Scanner;
//Payment method
//Random PaymentID
//Add shirt,remove shirt (Seller)

//Send money
//Register/Login
//Search
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
}
//1-10;
class Buyer extends User{
    private List<Product> cart;
    Buyer(String name, double balance, String email){
        super(name, balance, email);
        this.cart = new ArrayList<>();
    }
  
   void add_to_cart(Product item){
        cart.add(item);
   };
   public void list_cart(){
        System.out.println("ID  |   Product name    |   Price");
        for (int i = 0 ; i < cart.size(); i++) {
            System.out.printf("%-2d  |   %-2s    |   %-2d", i + 1, cart.get(i).name, cart.get(i).price );
        }
    };
    boolean check_cart(){
        if (cart.isEmpty()){
            return false;
        }
        else {return true;}
    }
    void buy(){
        //payment_method 
        if (check_cart() == false){
            System.out.println("Your cart are Empty");
            return;
        }
        double total = 0 ;
        for (Product item : cart) {
            total += item.price; 
        } 
   
        if (balance < total) {
             System.out.println("Purchase failed you need " + (total - balance) + " bath");
        }
        else {
            balance -= total;
            process_payment(balance);    
        }
    };
}

class Seller extends User{
    void add_shirt(Product item){
        list.add(item);
    };
    //void rm_shirt();
    Seller(String name, double balance, String email){
        super(name, balance, email);
    }
}

class Product{
    int product_id;
    String name, brand, size, quality;
    int price;

}

class Market{
    public ArrayList<Product> list = new ArrayList<>();
    public void list_shirt(){
        System.out.println("ID  |   Product name    |   Price");
        for (int i = 0 ; i < list.size(); i++) {
            System.out.printf("%-2d  |   %-2s    |   %-2d", i + 1, list.get(i).name, list.get(i).price );
        }
    };
}

class Payment{
   private int payment_id;
   private String[] payment_method ;
   private boolean paid_status = false ;
   //void  getDetail();
   private String payment_date;
   int process_payment(int balance){
    send_money()
   }
}


class main{
public static void main(String[] args){
    Scanner in = new Scanner(System.in);
    String role;

    System.out.print("Enter role (Buyer, Seller): ");
    role = in.nextLine(); 
        System.out.println(role);

    if(role.equals("Buyer") || role.equals("Seller")){
    System.out.println("You are " + role);
    }
    else {
    System.out.println("Your input not correct");
    }
    in.close();
    
   
}

}
