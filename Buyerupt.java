import java.util.*;
class Buyer extends User{
    private List<Product> cart;
    Buyer(String name, double balance, String email){
        super(name, balance, email);
        this.cart = new ArrayList<>();
    }
  
   void add_to_cart(Product item){
        cart.add(item);
   };
   void remove_cart(Product item){
    cart.remove(item);
   }
   public void list_cart(){
    System.out.println("ID  |   Product name    |   Price");
    for (int i = 0 ; i < cart.size(); i++) {
        Product p = cart.get(i);
        System.out.printf("%-2d  |   %-15s |   %.2f\n", i + 1, p.brand, p.price);
    }
}

    boolean check_cart(){
        if (cart.isEmpty()){
            return false;
        }
        else {return true;}
    }
    @Override
    void show_menu(Market market, Scanner in){
        while(true){
        System.out.println("\n=== MAIN MENU ===");
            System.out.println("Welcome Buyer");
            System.out.println("1. View product ");
            System.out.println("2. Cart Details ");
            System.out.println("3. Search Product ");
            System.out.println("4. User Details");
            System.out.println("0. Exit");
            System.out.print("Choose: ");
            int ch = in.nextInt();
            in.nextLine();
            switch (ch) {
                case 1: {  //view prodcut
                    market.list_shirt();
                    break;}
    
                case 2: // Search product
                    System.out.print("Search Product: ");
                    String target = in.nextLine();
                    market.search(target);
                    int index = market.search(target);
                    if(index == -1) {
                    System.out.println("Product not found!");
                    } else {
                    Product found = market.list.get(index);
                    System.out.println("Found: " + found.brand + " price: " + found.price);
                    }
                    break;

                case 3: // cart details
                    while(true) {
                        System.out.println("\n=== CART MENU ===");
                        System.out.println("1. Add to Cart");
                        System.out.println("2. View Cart");
                        System.out.println("0. Back to Main Menu");
                        System.out.print("Choose: ");
                        int cartChoice = in.nextInt();
                                in.nextLine();

                                if(cartChoice == 0) break; // กลับเมนูหลัก

                                switch(cartChoice) {
                                    case 1:
                                        System.out.print("Product name to add: ");
                                        target = in.nextLine();

                                        index = market.search(target);
                                        if(index == -1){
                                            System.out.println("Product not found!");
                                        } else {
                                            Product found = market.list.get(index);
                                            this.add_to_cart(found);
                                            System.out.println("Item has been added to cart!");
                                        }
                                        break;

                                    case 2:
                                        if(check_cart()) {
                                            this.list_cart();
                                        } else {
                                            System.out.println("Your cart is empty!");
                                        }
                                        break;

                                    default:
                                        System.out.println("Invalid choice, try again.");
                                }
                            }
                            break;

                case 4:{ // User details
                    this.display_detail();
                }
                
            }
            if(ch == 0){ //exit
                System.out.println("Bye!");
                break;
            }
            System.out.println("Press any key to continue...");
                    try {
                        System.in.read();
                    } catch (Exception e){}
        }
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
