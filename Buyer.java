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
            System.out.printf("%-2d  |   %-2s    |   %-2d", i + 1, cart.get(i).brand, cart.get(i).price );
        }
    };
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
            System.out.println("Welcome New Seller");
            System.out.println("1. View product ");
            System.out.println("2. View cart ");
            System.out.println("3. Search Product ");
            System.out.println("0. Exit");
            System.out.print("Choose: ");
            int ch = in.nextInt();
            in.nextLine();
            switch (ch) {
                case 1: {
                    market.list_shirt();
                    break;}
    
                case 2:
                    System.out.println("This is all product in Market");
                    break;
                
                case 3:
                    System.out.println("Search Product: ");
                    break;    
            }
            if(ch == 0){
                System.out.println("Bye!");
                break;
            }
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
