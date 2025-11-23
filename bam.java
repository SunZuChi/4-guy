import java.util.Scanner;
class User{
    String name;
    protected int balance;
    String email;
    User(String name, int balance ){
        this.name = name;
        this.balance = balance;
    }
    void display_detail(){
       System.out.println("Name: " + name + "\nBalance: " + balance);
    }
    void add_money(int money){
        balance +=money;
    }
}

class Buyer extends User{
    Buyer(String name, int balance){
        super(name, balance);
    }
   // void add_to_cart();
   // void buy();
}


class Seller extends User{
    //void add_shirt();
    //void rm_shirt();
    Seller(String name, int balance){
        super(name, balance);
    }
}

class Product{
    int product_id;
    String brand, size, quality;
    int price;

}

class Market{
    int Orderid;
    String[] shirt;
    //datetime
   // void list_shirt();
}

class Payment{
   private int payment_id;
   private String[] payment_method ;
   private boolean paid_status = false ;
   //void  getDetail();
   private String payment_date;
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
    User ice = new User("ICE", 500);
    in.close();
    
    ice.add_money(500);
    ice.display_detail();
}

}