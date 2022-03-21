
import java.util.*;
import java.util.concurrent.*;

public class Inferno {

    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("usage: java [hunger > 0]");
            System.exit(1);
        }

        final int DEVILS = 5;
        final int HUNGER = Integer.parseInt(args[0]);

        if (HUNGER < 1) {
            System.out.println("Cannot eat less than one time");
            System.exit(1);
        }

        Monitor monitor = new Monitor(DEVILS);

        ExecutorService ex = Executors.newFixedThreadPool(DEVILS);

        for (int i = 0; i < DEVILS; i++) {
            int id = i+1;
            int servantFor = (id % 5) + 1; /* circular dependency */
            ex.execute(new Thread(new Devil(monitor, id, servantFor, HUNGER)));
        }
        ex.shutdown();
        try {
            if (!ex.awaitTermination(60, TimeUnit.SECONDS)) {
                System.out.println("ERROR: COULD NOT FINISH PROGRAM");
                ex.shutdownNow();
            }
        } catch (InterruptedException e) {
            ex.shutdownNow();
            Thread.currentThread().interrupt();
        }

        System.out.println("-----------------------------------");
        System.out.println("The order people got to eat in: ");
        HashMap<Integer, LinkedList<Integer>> fairness = new HashMap<>();
        int i = 1;
        for (int id : monitor.eatingOrder) {
            if (fairness.containsKey(id)) {
                fairness.get(id).add(i);
            } else {
                LinkedList<Integer> l = new LinkedList<>();
                l.add(i);
                fairness.put(id, l);
            }
            i++;
        }

        for (int id : fairness.keySet()) {
            LinkedList<Integer> list = fairness.get(id);
            System.out.print("Devil #" + id + ": [ ");
            for (int pos : list) System.out.printf("%2d, ", pos);
            int diff = (list.getLast() - list.getFirst());
            System.out.printf("] time between first and last meal: %d\n", diff);
        }
    }
}
