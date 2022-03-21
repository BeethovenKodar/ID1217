
import java.util.*;

public class Monitor {

    private final int devilsEating;
    private final int MAX_EATERS;
    private final List<Integer> doneEating;
    private final List<Integer> waitToEat;
    public final List<Integer> eatingOrder;

    public Monitor(int noDevils) {
        this.devilsEating = noDevils;
        this.MAX_EATERS = noDevils / 2;
        doneEating = Collections.synchronizedList(new ArrayList<>());
        waitToEat = Collections.synchronizedList(new ArrayList<>());
        eatingOrder = Collections.synchronizedList(new ArrayList<>());
    }

    /* request to get fed by another thread */
    public synchronized boolean eatFood(int id) {
        int avoid_id = (id % 5) + 1;
        if (waitToEat.size() > 0 && waitToEat.get(0) == avoid_id) return false;

        if (waitToEat.size() < MAX_EATERS) {  /* avoid all waiting for food ==> deadlock */
            waitToEat.add(id);
            System.out.println("Devil #" + id + " waiting to be served");
            try { wait(); } catch (InterruptedException ie) {
                System.out.println("### Devil #" + id + " uncomfortably tickled during wait");
            }
            return true;
        } else return false;
    }

    /* serve food if correct thread (servantFor) is next in line */
    public synchronized boolean serveFood(int id, int servantFor) throws DinnerDoneException {
        if (waitToEat.size() > 0 && waitToEat.get(0) == servantFor) { /* if thread's eater is next in cond queue */
            int removed_id = waitToEat.remove(0);
            System.out.println("Devil #" + id + " served Devil #" + removed_id);
            eatingOrder.add(removed_id);
            notify();
            return true;
        } else if (doneEating.size() == devilsEating) /* all done eating, signal them to exit */
            throw new DinnerDoneException("### Devil #" + id + " left the table");
        else {
            if (waitToEat.size() > 0) System.out.println("I'm " + id + ", " + waitToEat.get(0) + " is next in line");
            return false; /* thread's target to feed isn't next to eat */
        }
    }

    /* called when a thread is done eating */
    public void markDone(int id) {
        doneEating.add(id);
    }
}
