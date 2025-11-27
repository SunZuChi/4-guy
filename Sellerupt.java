import java.util.*;
class Seller extends User{
    Scanner scanner = new Scanner(System.in);
    //void rm_shirt();
    boolean isre = false;
    Seller(String name, double balance, String email){
        super(name, balance, email);
    }
    public void recieve(double balance){
        this.balance += balance;
    }
    @Override
    void show_menu(Market market, Scanner in){
        while(true){
        System.out.println("\n=== MAIN MENU ===");
            System.out.println("Welcome Seller");
            System.out.println("Welcome Seller");
            System.out.println("1. Add Shirt");
            System.out.println("2. View product ");
            System.out.println("3. Search Product ");
            System.out.println("4. User Details");
            System.out.println("0. Exit");
            System.out.print("Choose: ");
            int ch = in.nextInt();
            in.nextLine();
            switch (ch) {
                case 1: 
                    market.list_shirt();
                    System.out.print("add/remove(rm): ");
                    String tempolaly = in.nextLine();
                    if(tempolaly.equalsIgnoreCase("add")){
                        System.out.print("Product brand : ");
                        String brand = in.nextLine();

                        System.out.print("Product size : ");
                        String size = in.nextLine();

                        System.out.print("Product quality : ");
                        String qua = in.nextLine();

                        System.out.print("Product price : ");
                        while (!in.hasNextDouble()){
                            System.out.println("Invalid input");
                            in.nextLine();
                        }
                        double price = in.nextDouble();
                        in.nextLine();

                        Product p = new Product(this, brand, size, qua, price);
                        market.add_product(p);
                        System.out.println("Add product complete");
                    }
                    else if (tempolaly.equalsIgnoreCase("rm")){
                        System.out.print("Remove product (ID) : ");
                        int product_id = in.nextInt();
                        in.nextLine();
                        market.remove_product(this, product_id);
                    }
                    break; 
    
                case 2:
                    System.out.println("This is all product in Market");
                    break;
                
                case 3:
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
                
                default:
                      System.out.println("Invalid!");
                case 4:{
                    this.display_detail();
                }

            }
            if(ch == 0){
                System.out.println("Bye!");
                break;
            }
            System.out.println("Press any key to continue...");
                    try {
                        System.in.read();
                    } catch (Exception e){}
        }
}
}