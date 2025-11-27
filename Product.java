class Product{
    int product_id;
    int next = 0;
    Seller seller;
    String brand, size, quality;
    double price;
    

     Product(Seller seller, String brand, String size, String quality,double price) {
        this.product_id = next++;
        this.seller = seller;
        this.brand = brand;
        this.size = size ;
        this.quality = quality;
        this.price = price;
        
    }
    
}