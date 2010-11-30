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

struct look_data {
	struct list_head queue;
	//struct list_head *head_entry;
	sector_t cur_sec;
	int head_direction;
};

/*
* is this needed?
*/
//
//static void look_merged_requests(struct request_queue *q, struct request *rq,
//				 struct request *next)
//{
//	list_del_init(&next->queuelist);
//}

static int look_dispatch(struct request_queue *q, int force)
{
	struct list_head *pos;
	struct request *tmp;
	struct look_data *nd = q->elevator->elevator_data;

	if (!list_empty(&nd->queue)) {
		//struct request *rq;
		//pick from head instead (rhymes)
		//rq = list_entry(nd->queue.next, struct request, queuelist);
		if(nd->head_direction == 1){ //ascending
			list_for_each(pos, &nd->queue){
				tmp = list_entry(pos, struct request, queuelist);
				if(tmp->bio->bi_sector >= nd->cur_sec){
					nd->cur_sec = blk_rq_pos(tmp) + blk_rq_sectors(tmp);
					break;
				}
			}	
		}
		else{ //descending
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
		if(tmp->bio->bi_sector < tmp->bio->bi_sector){
			list_add_tail(&rq->queue_list, pos);
			inserted = 1;	
			break;
		}
	}

	//request should be the new largest, add it
	if(!inserted)
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

	nd->cur_sec = 0;
	// Ascending = 1
	// Descending = 0
	nd->head_direction = 1;

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


MODULE_AUTHOR("Ian Crawford, Sarah Clisby, Matt Martinson, Matt Thomas");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LOOK IO scheduler");
