/*
 * Team 2
 * Ian Crawford
 * Sarah Clisby
 * Matt Martinson
 * Matt Thomas
 *
 * DESCRIPTION OF CHANGES HERE
 *
 * elevator look
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

/**
 * struct look_data - Tracks request list, disk head position and
 * direction.
 *
 * @queue: The request list.
 * @cur_sec: The current disk head position.
 * @head_direction: The current direction of the disk head.
 * 
 * This structure keeps track of the request list, as well as the
 * current position of the disk head and the direction (increasing
 * block number or decreasing block number) the disk head is
 * traveling.
 */
struct look_data {
	struct list_head queue;
	//struct list_head *head_entry;
	sector_t cur_sec;
	int head_direction;
};

static void look_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

/**
 * look_dispatch - Dispatches next request from request list.
 *
 * @*q - the request list
 * @force - ???
 *
 * This function checks the direction of disk head travel and
 * dispatches the next request in the list. If the disk head is
 * ascending, the request with the next highest block number is
 * dispatched. If the disk head is descending, the request with the
 * next lowest block number is dispatched. If the disk head has
 * dispatched the highest or lowest request, the disk head
 * direction is reversed.
 */
static int look_dispatch(struct request_queue *q, int force)
{
	struct list_head *pos;
	struct request *tmp;
	struct look_data *nd = q->elevator->elevator_data;

	if (!list_empty(&nd->queue)) {
		//struct request *rq;
		//pick from head instead (rhymes)
		//rq = list_entry(nd->queue.next, struct request, queuelist);
		if(nd->head_direction == 1){ // Disk head ascending
			list_for_each(pos, &nd->queue){
				tmp = list_entry(pos, struct request, queuelist);
				if(tmp->bio->bi_sector >= nd->cur_sec){
					nd->cur_sec = blk_rq_pos(tmp) + blk_rq_sectors(tmp);
					break;
				}
			}	
		}
		else{ // Disk head descending
			list_for_each_prev(pos, &nd->queue){
				tmp = list_entry(pos, struct request, queuelist);
				if(tmp->bio_bi_sector <= nd->cur_sec){
					nd->cur_sec = blk_rq_pos(tmp);
					break;
				}
			}
		}

		if(nd->head_direction == 1 &&  tmp->queuelist.next == nd->queue)
			nd->head_direction = 0;
		else if(nd->head_direction == 0 && tmp->queuelist.prev == nd->queue)
			nd->head_direction = 1;

		list_del_init(&tmp->queuelist);
		elv_dispatch_add_tail(q, tmp);
		return 1;
	}
	return 0;
}

/**
 * look_add_request - Adds a new request to the request list.
 *
 * @*q - The request list.
 * @*rq - The request to add.
 * 
 * This function adds a new request to the list of requests. The
 * block number of the new request is compared to the block numbers
 * of list entries, and the new request placed in the correct order.
 * This ensures that the list is sorted by increasing block number.
 */
static void look_add_request(struct request_queue *q, struct request *rq)
{
	struct list_head *pos;
	struct request *tmp;
	struct look_data *nd = q->elevator->elevator_data;

	int inserted = 0;

	//sort entry into list
	//list_add_tail(&rq->queuelist, &nd->queue);
	
	//try to add request before next largest
	list_for_each(pos, &nd->queue){
		tmp = list_entry(pos, struct request, queueList );
		if(tmp->bio->bi_sector < tmp->bio->bi_sector){ //What?
			list_add_tail(&rq->queue_list, pos);
			inserted = 1;	
			break;
		}
	}

	//request should be the new largest, add it
	if(!inserted)
		list_add_tail(&rq->queuelist, &nd->queue);	
}

/**
 * look_queue_empty - Checks if there are no more requests in the list.
 *
 * @*q - The request list.
 */
static int look_queue_empty(struct request_queue *q)
{
	struct look_data *nd = q->elevator->elevator_data;

	return list_empty(&nd->queue);
}

/**
 * look_former_request - Returns the previous request in the list.
 * 
 * @*q - The request list.
 * @*rq - The current request.
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
 * look_latter_request - Returns the next request in the list.
 *
 * @*q - The request list.
 * @*rq - The current request.
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
 * look_init_queue - Initializes the request list and disk head.
 * 
 * @*q - The request list.
 * 
 * Allocates memory for the request list and initializes the disk
 * head position and direction (default is ascending from block
 * number 0).
 */
static void *look_init_queue(struct request_queue *q)
{
	struct look_data *nd;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd)
		return NULL;
	INIT_LIST_HEAD(&nd->queue);

	nd->cur_sec = 0;
	// Ascending = 1
	// Descending = 0
	nd->head_direction = 1;

	return nd;
}

/**
 * look_exit_queue - Deallocates request list and disk head.
 * 
 * @*e - Elevator queue.
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


MODULE_AUTHOR("Ian Crawford, Sarah Clisby, Matt Martinson, Matt Thomas");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LOOK IO scheduler");
