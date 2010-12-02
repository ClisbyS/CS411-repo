/*
 * Elevator Look, names go next
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct look_data {
	struct list_head queue;
	sector_t cur_pos;
	unsigned int dir;	//1 for up
				//0 for down
};

static void look_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

/*
 * Loop through request queue, dispatch the correct request.
 */
static int look_dispatch(struct request_queue *q, int force)
{
	struct look_data *nd = q->elevator->elevator_data;
	struct request *rq, *higher = NULL, *lower = NULL;
	struct list_head *pos;

	//printk( "[LOOK] Entering Dispatch\n" ); // Antiquated
	
	// Only dispatch if the request queue is not empty
	if (!list_empty(&nd->queue)) {
		//struct request *rq, *higher, *lower; // Declared outside if now

		// Find next higher and lower requests based 
		// on current head location
		list_for_each( pos, &nd->queue ) {
		//while( nd->queue.next != NULL ) {

			rq = list_entry( pos, struct request, queuelist );
			//rq = list_entry(nd->queue.next, struct request, queuelist);
		
			// Is this request the better higher one?
			if( blk_rq_pos( rq ) > nd->cur_pos ) {
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
			if( blk_rq_pos( rq ) < nd->cur_pos ) {
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
		// ALSO: REMOVE BRACKETS IF THIS CODE DOES NOT CHANGE!!!
		// 	silly conventions...
		/*	if( nd->dir == 1 && higher == NULL ) {
			nd->dir = 0;
		}
		if( nd->dir == 0 && lower == NULL ) {
			nd->dir = 1;
		}

		// FOR DEBUGGING PURPOSES ONLY
		if( higher == NULL ) {
			printk( "[LOOK] higher: NULL\n" );
		}
		else {
			printk( "[LOOK] higher: %llu\n", blk_rq_pos( higher ) );
		}
		if( lower == NULL ) {
			printk( "[LOOK] lower: NULL\n" );
		}
		else {
			printk( "[LOOK] lower: %llu\n", blk_rq_pos( lower ) );
		}

		// Dispatch request
		if( nd->dir == 1 ) {	// Going up!
			printk( "[LOOK] dsp %u %llu\n", nd->dir, blk_rq_pos( higher ) );
			list_del_init( &higher->queuelist );
			elv_dispatch_sort( q, higher );
		}
		if( nd->dir == 0 ) {	// Going down!
			printk( "[LOOK] dsp %u %llu\n", nd->dir, blk_rq_pos( lower ) );
			list_del_init( &lower->queuelist );
			elv_dispatch_sort( q, lower );
		}*/

		// FOR GREAT SILLY TESTING
		if( higher == NULL ) {
			printk( "[LOOK] dsp %u %llu\n", nd->dir, blk_rq_pos( lower ) );
			list_del_init( &lower->queuelist );
			elv_dispatch_sort( q, lower );
		}
		else {
			printk( "[LOOK] dsp %u %llu]n", nd->dir, blk_rq_pos( higher ) );
			list_del_init( &higher->queuelist );
			elv_dispatch_sort( q, higher );
		}

		//list_del_init(&rq->queuelist);
		//nd->cur_pos = blk_rq_pos(rq) + blk_rq_sectors(rq);
		//printk( "[LOOK] Cur Pos is %llu\n", nd->cur_pos );
		//elv_dispatch_sort(q, rq);
		return 1;

	}

	printk( "[LOOK] No request to dispatch\n" );

	return 0;

}

/*
 * Leaving add request as it was. Dispatch will search through the 
 * request queue to find the next request to dispatch.
 */
static void look_add_request(struct request_queue *q, struct request *rq)
{
	struct look_data *nd = q->elevator->elevator_data;

	printk( "[LOOK] add %u %llu\n", nd->dir, blk_rq_pos( rq ) );

	list_add_tail(&rq->queuelist, &nd->queue);
}

static int look_queue_empty(struct request_queue *q)
{
	struct look_data *nd = q->elevator->elevator_data;

	return list_empty(&nd->queue);
}

static struct request *
look_former_request(struct request_queue *q, struct request *rq)
{
	struct look_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
look_latter_request(struct request_queue *q, struct request *rq)
{
	struct look_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

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


MODULE_AUTHOR("Group 02");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Look IO scheduler");

