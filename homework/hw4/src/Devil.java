
import java.util.Random;

enum Action {
    EAT,
    SERVE
}

public class Devil implements Runnable {


    private static Monitor monitor;
    private final Random random;
    private Action nextAction;

    private final int id;
    private final int servantFor;
    private int hunger;
    private int totalServings = 0;

    public Devil(Monitor monitor, int id, int servantFor, int hunger) {
        Devil.monitor = monitor;
        this.id = id;
        this.servantFor = servantFor;
        this.hunger = hunger;
        this.random = new Random();

        if (id % 2 == 0) nextAction = Action.EAT;
        else nextAction = Action.SERVE;
    }

    @Override
    public void run() {
        System.out.println("### Devil #" + id + " joined the table");
        while (true) {
            //rest(id); /* optional */
            try {
                switch (nextAction) {
                    case EAT: {
                        if (monitor.eatFood(id)) {                      /* try to eat */
                            hunger--;
                            if (hunger == 0)                            /* last bite */
                                monitor.markDone(id);
                        } else {                                        /* could not eat */
                            System.out.println("Devil #" + id + " could not get food, will serve instead");
                        }
                        nextAction = Action.SERVE;
                        break;
                    }

                    case SERVE: {
                        if (monitor.serveFood(id, servantFor)) {        /* try to serve */
                            totalServings++;
                        } else {                                        /* could not serve */
                            System.out.println("Devil #" + id + " hunger = " + hunger + ", servings = " + totalServings);
                            System.out.println("Devil #" + id + " could not serve anyone, will try again");
                        }
                        if (hunger >= 1) nextAction = Action.EAT; /* moved from row 54, covers if servantFor is done eating */
                        break;
                    }
                }
            } catch (DinnerDoneException dde) {                         /* dinner is done, thrown by monitor */
                System.out.println(dde.getMessage());
                break;
            }
        }
        System.out.println("### Devil #" + id + " TOTAL SERVINGS = " + totalServings);
    }

    /* calling thread sleeps for random amount of time, 0.1 - 0.3 seconds */
    private void rest(int id) {
        System.out.println("Devil #" + id + " going to sleep");
        int sleepDuration = (random.nextInt(200)) + 100;
        try { Thread.sleep(sleepDuration); } catch (InterruptedException ie) {
            System.out.println("Devil #" + id + " uncomfortably tickled during sleep");
        }
        System.out.println("Devil #" + id + " woke up");
    }
}
