import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.Scanner;

class Horse {
    private String name;
    private int speed;
    private int stamina;
    private int power;
    private int gut;
    private int wit;

    public Horse(String name, int speed, int stamina, int power, int gut, int wit) {
        this.name = name;
        this.speed = speed;
        this.stamina = stamina;
        this.power = power;
        this.gut = gut;
        this.wit = wit;
    }

    public String getName() {
        return name;
    }

    public int getSpeed() {
        return speed;
    }

    public int getStamina() {
        return stamina;
    }

    public int getPower() {
        return power;
    }

    public int getGut() {
        return gut;
    }

    public int getWit() {
        return wit;
    }

    // ใช้ค่าของ stat ทั้งหมดในการคำนวณการวิ่งของม้า
    public double race(int trackLength) {
        double timeTaken = trackLength / (double) speed;

        // เพิ่มเวลาหรือคำนวณตามค่าของ Stamina, Power, Gut, Wit
        timeTaken *= (1 + (100 - stamina) / 100.0);  // Stamina มีผลในระยะทางยาว
        timeTaken -= (power / 1000.0);  // Power ช่วยให้เริ่มเร็ว
        timeTaken -= (gut / 1000.0);  // Gut ช่วยเร่งในช่วงสุดท้าย
        timeTaken += (wit / 2000.0);  // Wit ช่วยหลีกเลี่ยงอุปสรรค

        return timeTaken; // เวลาที่ใช้ในการแข่ง
    }
}

class Race {
    private List<Horse> horses = new ArrayList<>();
    private List<Player> players = new ArrayList<>();
    private int trackLength;

    public Race(int trackLength) {
        this.trackLength = trackLength;
    }

    public void addHorse(Horse horse) {
        horses.add(horse);
    }

    public void addPlayer(Player player) {
        players.add(player);
    }

    public void startRace() {
        System.out.println("Race start with track length: " + trackLength + " meters!");

        // สุ่มคำนวณเวลาการแข่งสำหรับม้าทุกตัว
        List<Double> times = new ArrayList<>();
        for (Horse horse : horses) {
            double time = horse.race(trackLength);
            times.add(time);
            System.out.println(horse.getName() + " completed the race in " + String.format("%.2f", time) + " seconds.");
        }

        // หาม้าผู้ชนะ (ม้าที่ใช้เวลาน้อยที่สุด)
        int winnerIndex = times.indexOf(Collections.min(times));
        Horse winnerHorse = horses.get(winnerIndex);
        System.out.println("Winner is " + winnerHorse.getName() + " with time " + String.format("%.2f", times.get(winnerIndex)) + " seconds.");

        // ประกาศผลการเดิมพัน
        for (Player player : players) {
            if (player.getSelectedHorse() == winnerHorse) {
                double reward = player.getBalance() * 2;  // ตัวอย่างการจ่ายรางวัล
                player.updateBalance(reward);
                System.out.println(player.getName() + " Win " + reward + " Baht");
            } else {
                System.out.println(player.getName() + " Lose bet!");
            }
        }
    }
}

class Player {
    private String name;
    private double balance;
    private Horse selectedHorse;

    public Player(String name, double balance) {
        this.name = name;
        this.balance = balance;
    }

    public String getName() {
        return name;
    }

    public double getBalance() {
        return balance;
    }

    public void setSelectedHorse(Horse horse) {
        this.selectedHorse = horse;
    }

    public Horse getSelectedHorse() {
        return selectedHorse;
    }

    public void updateBalance(double amount) {
        this.balance += amount;
    }

    public boolean placeBet(double betAmount) {
        if (betAmount > balance) {
            System.out.println("You cannot bet (not enough money)");
            return false;
        }
        balance -= betAmount;
        return true;
    }
}

public class horserace {
    public static void main(String[] args) {
        Random rand = new Random();
        Scanner scanner = new Scanner(System.in);

        // สร้างม้า 8 ตัว
        Horse[] horses = new Horse[8];
        for (int i = 0; i < 8; i++) {
            horses[i] = new Horse("Horse " + (i+1), rand.nextInt(1100) + 100, rand.nextInt(1100) + 100,
                    rand.nextInt(1100) + 100, rand.nextInt(1100) + 100, rand.nextInt(1100) + 100);
            System.out.println(horses[i].getName() + " - Speed: " + horses[i].getSpeed() + ", Stamina: " + horses[i].getStamina() +
                    ", Power: " + horses[i].getPower() + ", Gut: " + horses[i].getGut() + ", Wit: " + horses[i].getWit());
        }

        // สุ่มระยะทางสนาม
        int trackLength = rand.nextInt(2001) + 1200;  // สุ่มระหว่าง 1200m - 3200m
        System.out.println("The race track is " + trackLength + " meters long.");

        // สร้างผู้เล่น
        Player player1 = new Player("Boi", 1000);
        Player player2 = new Player("Girl", 1000);

        // แสดงรายการม้าให้ผู้เล่นเลือก
        System.out.println("Player 1, choose a horse (enter number 1-8): ");
        for (int i = 0; i < horses.length; i++) {
            System.out.println((i+1) + ". " + horses[i].getName());
        }
        int choice1 = scanner.nextInt();
        player1.setSelectedHorse(horses[choice1 - 1]);  // เลือกม้าตามหมายเลขที่ผู้เล่นเลือก

        System.out.println("Player 2, choose a horse (enter number 1-8): ");
        int choice2 = scanner.nextInt();
        player2.setSelectedHorse(horses[choice2 - 1]);  // เลือกม้าตามหมายเลขที่ผู้เล่นเลือก

        // สร้างการแข่งขัน
        Race race = new Race(trackLength); 
        for (Horse horse : horses) {
            race.addHorse(horse);
        }
        race.addPlayer(player1);
        race.addPlayer(player2);

        // ผู้เล่นวางเดิมพัน
        System.out.println("Player 1, place your bet: ");
        double bet1 = scanner.nextDouble();
        player1.placeBet(bet1);

        System.out.println("Player 2, place your bet: ");
        double bet2 = scanner.nextDouble();
        player2.placeBet(bet2);

        // เริ่มการแข่งขัน
        race.startRace();
    }
}
