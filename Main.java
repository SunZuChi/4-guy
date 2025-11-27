import java.util.*;

//Payment method
//Random PaymentID
//Add shirt,remove shirt (Seller)

//Send money
//Register/Login
//Search
class main{
public static void main(String[] args){
    Market market = new Market();
    Scanner in = new Scanner(System.in);
    
   //------------------------Initial Data-------------------------------//
    List<Product> products = new ArrayList<>();
    Seller s1 = new Seller("Nameless", 5000.0, "alice@example.com");
    Seller s2 = new Seller("Ronaldo", 3000.0, "bob@example.com");
    market.users.put("Nameless", s1);
    market.users.put("Ronaldo", s2);
    products.add(new Product(s1, "Nike", "M", "Excellent", 1200.0));
    products.add(new Product(s2, "Adidas", "L", "Good", 900.0));
    products.add(new Product(s1, "Uniqlo", "S", "Like New", 500.0));
    products.add(new Product(s2, "H&M", "XL", "Fair", 350.0));
    products.add(new Product(s1, "Zara", "M", "Very Good", 700.0));
    for (Product pt : products){
        market.add_product(pt);
    }
//----------------------------------------------------------------------//

    while (true) {   // <-- ใช้ while loop ตรงนี้
        System.out.println("Register(r) | Login(l) | Guest(q) | Exit(x)");
        System.out.print("Type here : ");
        String choice = in.nextLine(); 
        User cr = null;

        switch (choice) {
            case "r":
                cr = market.register(in);
                  break;
            case "l":
                cr = market.login(in);
                  break;
            case "q":
                market.guest(in);
                cr = null;
                break;
            case "x":
                 System.out.println("Exit from program");
                 in.close();
                 return;
               
            default:
                System.out.println("Invalid input!");
        }
        
        if (cr instanceof Seller){
            cr.show_menu(market, in);
        }
        else if (cr instanceof Buyer){
            cr.show_menu(market, in);
            }
        }   
    }   
}









