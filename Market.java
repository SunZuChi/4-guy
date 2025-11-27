import java.util.*;
class Market{
    public ArrayList<Product> list = new ArrayList<>();
    HashMap<String, User> users = new HashMap<>();
    
//--------------------------------------------------------------------------------------------------------------------------------------------
public User register(Scanner in) {
    User tem;
    System.out.print("Enter name: ");
    String name = in.nextLine();

    System.out.print("Enter balance: ");
    while (!in.hasNextDouble()){
        System.out.println("Invalid!");
        in.nextLine();
    }
    double bal = in.nextDouble();

    in.nextLine();

    System.out.print("Enter email: ");
    String email = in.nextLine();

    System.out.print("Are you buyer(b) or seller(s)? ");
    String role = in.nextLine();

    if(role.equalsIgnoreCase("b")) {
        tem = new Buyer(name, bal, email);
    } else {
        tem = new Seller(name, bal, email);  
    }
    
    users.put(name , tem);
    return tem;
}
public User login(Scanner in){
    System.out.print("Enter name : ");
    String name = in.nextLine();

    if (!users.containsKey(name)){
       System.out.println("User does not exist. Register first."); 
       return null;
        }
    else {
    User u = users.get(name);
    System.out.println("Login successful! Welcome " + u.name);
    return u;
        }
}
 void guest(Scanner in){
    System.out.print("You are Guest. Not allow for add cart, add shirt");
    }
//-------------------------------------------------------------------------------------------------------------------------------------------------------------
    void add_product(Product item){list.add(item);}
    void remove_product(Seller seller, int product_id)
    {
    Iterator<Product> it = list.iterator();
    while(it.hasNext()){
        Product p = it.next();
        if (p.product_id == product_id && p.seller == seller){
             it.remove();
             System.out.println("Remove Successful");
             return;
            }

        }
    }

    public void list_shirt(){
        System.out.println("ID   |   Product name  |   Price");
        for (int i = 0 ; i < list.size(); i++) {
            System.out.printf("%-3d  |   %-10s    |   %7.2f\n", list.get(i).product_id , list.get(i).brand, list.get(i).price);
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
