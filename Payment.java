import java.util.*;
class Payment{
   Buyer buyer ;
   boolean paid_status;
   Payment (Buyer buyer){
        this.buyer = buyer;
        paid_status = false;
    }
   boolean process_payment(HashMap<Seller, Double> paymap){
    double total = 0;
        for (double amount : paymap.values()) {
            total += amount;       
        } 
        if (buyer.balance < total){
            System.out.println("You need " + (total - buyer.balance) + " to complete payment");
            return false;
        }
        
            buyer.dec_balance(total);
            for (Seller seller : paymap.keySet()){
                double amount = paymap.get(seller);
                seller.recieve(amount);
                
            }
        paid_status = true;
        System.out.println("Complete payment!");
        return true;
   }
};