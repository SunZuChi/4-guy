import java.util.*;

class Buyer extends User {
    private List<Product> cart = new ArrayList<>();

    Buyer(String name, double balance, String email) {
        super(name, balance, email);
    }

    double get_balance() { return balance; }
    void dec_balance(double amount) { 
        balance -= amount; 
    }

    Product add_cart(int id, Market market){
        for (Product p : market.list){
            if (p.product_id == id){
                return p;
            }
        }
        return null;
    }

    void remove_cart(Product item) { cart.remove(item); }

    public void list_cart() {
        if(!check_cart()){
            System.out.println("cart is empty now");
            return;
        }
        System.out.println("ID  | Product name   | Price");
        for (int i = 0; i < cart.size(); i++) {
            Product p = cart.get(i);
            System.out.printf("%-3d | %-12s | %8.2f\n", p.product_id, p.brand, p.price);
        }
    }

    boolean check_cart() {return !cart.isEmpty(); }

    @Override
    void show_menu(Market market, Scanner in) {
        while (true) {
            System.out.println("\n=== MAIN MENU ===");
            System.out.println("Welcome Buyer");
            System.out.println("1. View product");
            System.out.println("2. Search Product");
            System.out.println("3. Cart Details");
            System.out.println("4. User Details");
            System.out.println("0. Exit");
            System.out.print("Choose: ");
            int ch = in.nextInt();
            in.nextLine();

            switch (ch) {
                case 1:
                    market.list_shirt();
                    while (true){
                        System.out.print("Enter Product ID to add: ");
                        int id = in.nextInt();
                        in.nextLine();

                        if (id == 0){break;}
                        Product p = add_cart(id, market);
                        if (p != null) {
                        cart.add(p);
                        }
                        else {
                          System.out.println("Product not found");
                         }
                        
                }
                break;
                case 2:
                    System.out.print("Search Product: ");
                    String target = in.nextLine();
                    int index = market.search(target);
                    if (index == -1) System.out.println("Product not found!");
                    else {
                        Product found = market.getProduct(index);
                        System.out.println("Found: " + found.brand + " price: " + found.price);
                    }
                    break;

                case 3:
                    
                    while (true) {
                        list_cart();
                        System.out.println("\n=== CART MENU ===");
                        System.out.println("1. remove from Cart");
                        System.out.println("2. Do purchase");
                        System.out.println("0. Back");
                        System.out.print("Choose: ");
                        int cartChoice = in.nextInt();
                        in.nextLine();

                        if (cartChoice == 0) break;

                        switch (cartChoice) {
                            case 1:
                                if(!check_cart()) {
                                    System.out.println("Your cart is empty!");
                                    break;
                                }
                                list_cart();
                                while (true){
                                    System.out.print("Enter the ID of the product to remove: ");
                                    int targetId = in.nextInt(); 
                                    in.nextLine(); 

                                    if (targetId == 0){break;}

                                    Product found = null;
                                    for (Product p : cart) {
                                        if (p.product_id == targetId) {
                                            found = p;
                                            break;
                                                }
                                 }
                                if (found != null) {
                                    cart.remove(found);
                                    System.out.println(found.brand + " has been removed from your cart!");
                                   }                          
                                else {
                                    System.out.println("Product not found in your cart!");
                                         }
                                }
                                break;
                            case 2:
                                this.buy(market);
                                break;
                            default:
                                System.out.println("Invalid choice!");
                        }
                    }
                    break;

                case 4:
                    display_detail();
                    break;

                case 0:
                    System.out.println("Bye!");
                    return;

                default:
                    System.out.println("Invalid choice!");
                    
            }
            System.out.println("Press any key to continue...");
                    try {
                        System.in.read();
                    } catch (Exception e){}
            
        }
    }

    void buy(Market market) {
        if (!check_cart()) {
            System.out.println("Your cart is empty!");
            return;
        }
        HashMap<Seller, Double> paymap = new HashMap<>();
        for (Product item : cart) {
            paymap.put(item.seller, paymap.getOrDefault(item.seller, 0.0) + item.price);
        }
        Payment new_pay = new Payment(this);
        if (new_pay.process_payment(paymap)) {
            for (Product item : cart) {
                market.remove_product(item.seller, item.product_id);
            }     
        }
        cart.clear();
    }
}
