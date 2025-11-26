import java.util.ArrayList;
import java.util.HashMap;
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
        HashMap<Seller, Double> paymap = new HashMap<>();
        double total = 0 ;
        for (Product item : cart) {
            total += item.price; 

            paymap.put(item.seller, paymap.getOrDefault(item.seller, 0.0) + item.price);
        } 
        
        if (balance < total) {
             System.out.println("Purchase failed you need " + (total - balance) + " bath");
        }
        else {
           balance -= total;

        for (Seller s : paymap.keySet()){
            double amount = paymap.get(s);
            new Payment(s).process_payment(amount);
            }
        }
    };
}

class Seller extends User{
    //void rm_shirt();
    Seller(String name, double balance, String email){
        super(name, balance, email);
    }
    public void recieve(double balance){
        this.balance += balance;
    }
}

class Product{
    int product_id;
    Seller seller;
    String name, brand, size, quality;
    int price;
}

class Market{
    private ArrayList<Product> list = new ArrayList<>();
    private ArrayList<User> user = new ArrayList<>();

    void add_product(Product item){list.add(item);}
    void remove_product(Product item){list.remove(item);}

    public void list_shirt(){
        System.out.println("ID  |   Product name    |   Price");
        for (int i = 0 ; i < list.size(); i++) {
            System.out.printf("%-2d  |   %-2s    |   %-2d", i + 1, list.get(i).name, list.get(i).price );
        }
    };
    int search(String target){
        for (int n = 0 ; n < list.size() ; n++){
            if (list.get(n) != null && list.get(n).equals(target)){
                return n;
            }
        }
        return -1;
    }
}

class Payment{
    Seller seller;
   private int payment_id;
   private String[] payment_method ;
   private boolean paid_status ;
   private String payment_date;
   Payment (Seller seller){
        this.seller = seller;
        paid_status = false;
    }
    //void  getDetail();
   void process_payment(double amount){
       seller.recieve(amount);
   }
};



class main{
   static HashMap<String, User> users = new HashMap<>();
   
public static void main(String[] args){
    System.out.println("Register(r) | Login(l) | Quest(q) ");
    Scanner in = new Scanner(System.in);
    System.out.print("Type here : ");
    String choice;
    
    
    choice = in.nextLine(); 
    if (choice.equals("r")){
        register(in);
    }
     if (choice.equals("l")){
        login(in);
    }
    if (choice.equals("q")){
        quest(in);
    }

    in.close();
}
static void register(Scanner in){
    System.out.println("Set name : ");
        String name = in.nextLine();

        System.out.println("Set Balance : ");
        double balance = in.nextDouble();

        in.nextLine();

        System.out.println("Set Email : ");
        String email = in.nextLine();

        System.out.println("Set Role(Buyer(b), Seller(s)) : ");
        String role = in.nextLine();
        
         User user = null;

        if (role.equalsIgnoreCase("b")){
            user = new Buyer(name,balance,email);
        }
        else if (role.equalsIgnoreCase("s")){
            user = new Seller(name,balance,email);
        }
        else { System.out.println("Invalid Tried again");}

      users.put(name, user);
}
static void login(Scanner in){
    System.out.println("Enter name : ");
    String name = in.nextLine();

    if (!users.containsKey(name)){
       System.out.println("User does not exist. Register first."); 
       return;
        }
     User u = users.get(name);
     System.out.println("Login successful! Welcome " + u.name);
    }
static void quest(Scanner in){
    System.out.print("You are Guest. Not allow for add cart, add shirt");
    }
}
