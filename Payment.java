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