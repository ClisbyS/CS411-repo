/*
 * Elevator Look Scheduler
 * Team 2
 * Ian Crawford, Sarah Clisby, Matt Martinson, Matt Thomas
 * 
 * We changed the data structure look_data to keep track of
 * the disk head position and motion of direction. The other
 * changes were made in look_dispatch, which searches through
 * the request list and finds the next largest request block
 * if the head is moving up, and the next smallest request
 * block if the head is moving down.
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

/**
 * struct look_data - Keeps track of the list head, disk head position and direction.
 * 
 * @queue: the list head
 * @cur_pos: the current position of the disk head
 * @dir: the direction the disk head is traveling
 */
struct look_data {
	struct list_head queue;
	sector_t cur_pos;
	unsigned int dir;	//1 for up
				//0 for down
};

/**
 * look_merged_requests - Unchanged in this project.
 */
static void look_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

/**
 * look_dispatch - Loops through the request queue, dispatches the correct request.
 * @*q: the request queue
 * @force: unused variable
 *
 * The search starts at the head of the list and searches for the request with the
 * smallest block number higher than the current disk head position and the request
 * with the largest block number lower than the current disk head position. It then
 * checks to see if the disk head needs to switch directions (if it has already
 * dispatched the highest or lowest request in the list) and dispatches the proper
 * request (higher or lower).
 */
static int look_dispatch(struct request_queue *q, int force)
{
	struct look_data *nd = q->elevator->elevator_data;
	struct request *rq, *higher = NULL, *lower = NULL;
	struct list_head *pos;
	
	// Only dispatch if the request queue is not empty
	if (!list_empty(&nd->queue)) {

		// Find next higher and lower requests based 
		// on current head location
		list_for_each( pos, &nd->queue ) {

			rq = list_entry( pos, struct request, queuelist );
		
			// Is this request the better higher one?
			if( blk_rq_pos( rq ) >= nd->cur_pos ) {
				if( higher != NULL ) {
					if( blk_rq_pos( rq ) < blk_rq_pos( higher ) ) {
						higher = rq;
					}
				}
				else {
					higher = rq;
				}
			}
			
			// Is this request the better lower one?
			if( blk_rq_pos( rq ) <= nd->cur_pos ) {
				if( lower != NULL ) {
					if( blk_rq_pos( rq ) > blk_rq_pos( lower ) ) {
						lower = rq;
					}
				}
				else {
					lower = rq;
				}
			}

		}

		// Switch directions if needed
		if( nd->dir == 1 && higher == NULL )
			nd->dir = 0;
		if( nd->dir == 0 && lower == NULL )
			nd->dir = 1;

		// Dispatch request
		if( nd->dir == 1 ) {
			printk( "[LOOK] dsp %u %llu\n", nd->dir, blk_rq_pos( higher ) );
			nd->cur_pos = higher->bio->bi_sector + bio_sectors(higher->bio);
			list_del_init( &higher->queuelist );
			elv_dispatch_sort( q, higher );
		}
		if( nd->dir == 0 ) {
			printk( "[LOOK] dsp %u %llu\n", nd->dir, blk_rq_pos( lower ) );
			nd->cur_pos = lower->bio->bi_sector + bio_sectors(lower->bio);
			list_del_init( &lower->queuelist );
			elv_dispatch_sort( q, lower );
		}

		return 1;

	}

	printk( "[LOOK] No request to dispatch\n" );

	return 0;

}

/**
 * look_add_request - Adds a request to the request list.
 * @*q: request list
 * @*rq: request to add
 *
 * This function is unchanged, save for the addition of the
 * printk statement.  All the LOOK logic is implemented in
 * look_dispatch
 */
static void look_add_request(struct request_queue *q, struct request *rq)
{
	struct look_data *nd = q->elevator->elevator_data;

	printk( "[LOOK] add %u %llu\n", nd->dir, blk_rq_pos( rq ) );

	list_add_tail(&rq->queuelist, &nd->queue);
}

/**
 * look_queue_emtpy - This function is unchanged.
 */
static int look_queue_empty(struct request_queue *q)
{
	struct look_data *nd = q->elevator->elevator_data;

	return list_empty(&nd->queue);
}

/**
 * struct look_former_request - Returns the previous request.
 * This function is unchanged and unused.
 */
static struct request *
look_former_request(struct request_queue *q, struct request *rq)
{
	struct look_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

/**
 * struct look_latter_request - Returns the next request.
 * This function is unchanged and used.
 */
static struct request *
look_latter_request(struct request_queue *q, struct request *rq)
{
	struct look_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

/**
 * *look_init_queue - Initializes the request list.
 * @*q: the request list
 *
 * Initializes the request queue and the disk head position
 * and direction.
 */
static void *look_init_queue(struct request_queue *q)
{
	struct look_data *nd;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd)
		return NULL;
	INIT_LIST_HEAD(&nd->queue);
	nd->cur_pos = 0;
	nd->dir = 1;	//Initially going up!
	return nd;
}

/**
 * look_exit_queue - Frees memory allocated by look_init_queue.
 * @*e: elevator queue
 *
 * This function is unchanged.
 */
static void look_exit_queue(struct elevator_queue *e)
{
	struct look_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_look = {
	.ops = {
		.elevator_merge_req_fn		= look_merged_requests,
		.elevator_dispatch_fn		= look_dispatch,
		.elevator_add_req_fn		= look_add_request,
		.elevator_queue_empty_fn	= look_queue_empty,
		.elevator_former_req_fn		= look_former_request,
		.elevator_latter_req_fn		= look_latter_request,
		.elevator_init_fn		= look_init_queue,
		.elevator_exit_fn		= look_exit_queue,
	},
	.elevator_name = "look",
	.elevator_owner = THIS_MODULE,
};

static int __init look_init(void)
{
	elv_register(&elevator_look);

	return 0;
}

static void __exit look_exit(void)
{
	elv_unregister(&elevator_look);
}

module_init(look_init);
module_exit(look_exit);


MODULE_AUTHOR("Team 02");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Look IO scheduler");

