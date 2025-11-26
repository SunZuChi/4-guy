import java.util.ArrayList;
import java.util.List;
import java.util.Scanner;
import java.util.Random;
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
         //   process_payment(balance);    
        }
    };
}

class Seller extends User {
    private Market market;

    Seller(String name, double balance, String email) {
        super(name, balance, email);
    }

    void add_shirt(Product item, Market market){ // เพิ่ม Market เป็น parameter
        market.list.add(item);
        System.out.println("Added " + item.name + " to " + market.market_name);
    }

    void remove_shirt(Product item, Market market){ // ลบจากตลาดที่กำหนด
        market.list.remove(item);
        System.out.println("Removed " + item.name + " from " + market.market_name);
    }
}

class Product{
    int product_id;
    String name, brand, size, quality;
    int price;

}

class Market{
    public ArrayList<Product> list = new ArrayList<>();
    String market_name;
    Market(String market_name) {
        this.market_name = market_name;
    }
    // List products in this market
    public void list_products() {
        System.out.println("Market: " + market_name);
        System.out.println("ID  | Product name | Price");

        for (int i = 0; i < list.size(); i++) {
            System.out.printf("%-2d | %-10s | %-3d\n", i + 1, list.get(i).name, list.get(i).price);
        }
        System.out.println();
    }

    // Static method to list products in all markets
    public static void list_all_markets_products(ArrayList<Market> markets) {
        for (Market m : markets) {
            m.list_products(); // use each market's method
        }
    }


    public void list_shirt(){
        System.out.println("ID  |   Product name    |   Price   | " + market_name);
        for (int i = 0 ; i < list.size(); i++) {
            System.out.printf("%-2d | %-10s | %-3d\n", i + 1, list.get(i).name, list.get(i).price);
        }
    };

  

}

class Payment{
   private int payment_id;
   private String payment_method ;
   private boolean paid_status = false ;
   private String payment_date;
   //int process_payment(int balance){
    //send_money();
  // }
  Payment(int payment_id, String payment_method, boolean paid_status, String payment_date){
    this.payment_id = payment_id;
    this.payment_method = payment_method;
    this.paid_status = paid_status;
    this.payment_date = payment_date;
  }

  void  getDetail(){
        System.out.println("Payment ID: " + payment_id);
        System.out.println("payment Method: " + payment_method);
        System.out.println("Paid_Status: " + paid_status);
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
    
Payment ronaldo = new Payment(1234,"Bank transfer", false, "12/12/2024");   
ronaldo.getDetail();


ArrayList<Market> markets = new ArrayList<>();

Market m1 = new Market("SuperMart");
    Market m2 = new Market("MiniMart");
    markets.add(m1);
    markets.add(m2);

    // Create products
    Product p1 = new Product();
    p1.name = "Nike Shirt"; p1.price = 450;
    Product p2 = new Product();
    p2.name = "Adidas Shirt"; p2.price = 400;
    Product p3 = new Product();
    p3.name = "Puma Shirt"; p3.price = 350;

    Seller s = new Seller("John", 1000, "john@mail.com");
    
    s.add_shirt(p3, m2);
    
    Market.list_all_markets_products(markets);
    m2.list_shirt();
    s.remove_shirt(p3, m2);
    // List all products in all markets
    Market.list_all_markets_products(markets);

}

}
