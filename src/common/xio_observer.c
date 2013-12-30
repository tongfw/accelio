/*
 * Copyright (c) 2013 Mellanox Technologies®. All rights reserved.
 *
 * This software is available to you under a choice of one of two licenses.
 * You may choose to be licensed under the terms of the GNU General Public
 * License (GPL) Version 2, available from the file COPYING in the main
 * directory of this source tree, or the Mellanox Technologies® BSD license
 * below:
 *
 *      - Redistribution and use in source and binary forms, with or without
 *        modification, are permitted provided that the following conditions
 *        are met:
 *
 *      - Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *      - Neither the name of the Mellanox Technologies® nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "xio_os.h"
#include "xio_common.h"
#include "xio_observer.h"

/*---------------------------------------------------------------------------*/
/* xio_observer_create							     */
/*---------------------------------------------------------------------------*/
struct xio_observer *xio_observer_create(void *impl, notify_fn_t notify)
{
	struct xio_observer *observer;

	observer = kcalloc(1, sizeof(struct xio_observer), GFP_KERNEL);
	if (observer == NULL) {
		xio_set_error(ENOMEM);
		return NULL;
	}

	observer->impl		= impl;
	observer->notify	= notify;

	return observer;
}

/*---------------------------------------------------------------------------*/
/* xio_observer_destroy							     */
/*---------------------------------------------------------------------------*/
void xio_observer_destroy(struct xio_observer *observer)
{
	kfree(observer);
}

/*---------------------------------------------------------------------------*/
/* xio_observerable_create						     */
/*---------------------------------------------------------------------------*/
struct xio_observable *xio_observable_create(void *impl)
{
	struct xio_observable *observable;

	observable = kcalloc(1, sizeof(struct xio_observable), GFP_KERNEL);
	if (observable == NULL) {
		xio_set_error(ENOMEM);
		return NULL;
	}

	INIT_LIST_HEAD(&observable->observers_list);

	observable->impl = impl;

	return observable;
}

/*---------------------------------------------------------------------------*/
/* xio_observable_destroy						     */
/*---------------------------------------------------------------------------*/
void xio_observable_destroy(struct xio_observable *observable)
{
	kfree(observable);
}

/*---------------------------------------------------------------------------*/
/* xio_observable_unreg_all_observers					     */
/*---------------------------------------------------------------------------*/
static struct xio_observer_node *xio_observable_find(
				struct xio_observable *observable,
				struct xio_observer *observer)
{
	struct xio_observer_node *observer_node, *tmp_observer_node;

	if (observable->observer_node &&
	    observable->observer_node->observer == observer) {
		ERROR_LOG("already exist: " \
			  "observable:%p, observer:%p\n",
			  observable, observable->observer_node->observer);
		return observable->observer_node;
	}

	list_for_each_entry_safe(observer_node, tmp_observer_node,
				 &observable->observers_list,
				 observers_list_node) {
		if (observer_node->observer == observer) {
			ERROR_LOG("already exist: " \
				  "observable:%p, observer:%p\n",
				  observable, observer_node->observer);
			return observer_node;
		}
	}
	return NULL;
}

/*---------------------------------------------------------------------------*/
/* xio_observable_reg_observer						     */
/*---------------------------------------------------------------------------*/
void xio_observable_reg_observer(struct xio_observable *observable,
				 struct xio_observer *observer)
{
	struct xio_observer_node *observer_node;

	if (xio_observable_find(observable, observer)) {
		ERROR_LOG("double registration is forbidden\n");
		return;
	}

	observer_node = kcalloc(1,
				sizeof(struct xio_observer_node), GFP_KERNEL);
	if (observer_node == NULL) {
		xio_set_error(ENOMEM);
		return;
	}
	observer_node->observer = observer;

	if (list_empty(&observable->observers_list))
		observable->observer_node = observer_node;
	else
		observable->observer_node = NULL;

	list_add(&observer_node->observers_list_node,
		 &observable->observers_list);
}

/*---------------------------------------------------------------------------*/
/* xio_observable_unreg_observer					     */
/*---------------------------------------------------------------------------*/
void xio_observable_unreg_observer(struct xio_observable *observable,
				   struct xio_observer *observer)
{
	struct xio_observer_node *observer_node, *tmp_observer_node;


	list_for_each_entry_safe(observer_node, tmp_observer_node,
				 &observable->observers_list,
			observers_list_node) {
		if (observer == observer_node->observer) {
			if (observable->observer_node == observer_node)
				observable->observer_node = NULL;

			list_del(&observer_node->observers_list_node);
			kfree(observer_node);
			break;
		}
	}
}

/*---------------------------------------------------------------------------*/
/* xio_observable_notify_observer					     */
/*---------------------------------------------------------------------------*/
void xio_observable_notify_observer(struct xio_observable *observable,
				    struct xio_observer *observer,
				    int event, void *event_data)
{
	observer->notify(observer->impl, observable->impl, event, event_data);
}

/*---------------------------------------------------------------------------*/
/* xio_observable_notify_all_observers					     */
/*---------------------------------------------------------------------------*/
void xio_observable_notify_all_observers(struct xio_observable *observable,
					 int event, void *event_data)
{
	struct xio_observer_node *observer_node, *tmp_observer_node;

	if (observable->observer_node) {
		observable->observer_node->observer->notify(
				observable->observer_node->observer->impl,
				observable->impl, event, event_data);
		return;
	}

	list_for_each_entry_safe(observer_node, tmp_observer_node,
				 &observable->observers_list,
				 observers_list_node) {
		observer_node->observer->notify(
				observer_node->observer->impl,
				observable->impl, event, event_data);
	}
}

/*---------------------------------------------------------------------------*/
/* xio_observable_notify_any_observer					     */
/*---------------------------------------------------------------------------*/
void xio_observable_notify_any_observer(struct xio_observable *observable,
					int event, void *event_data)
{
	struct xio_observer_node *observer_node, *tmp_observer_node;

	if (observable->observer_node) {
		observable->observer_node->observer->notify(
				observable->observer_node->observer->impl,
				observable->impl, event, event_data);
		return;
	}

	list_for_each_entry_safe(observer_node, tmp_observer_node,
				 &observable->observers_list,
				 observers_list_node) {
		observer_node->observer->notify(
				NULL,
				observable->impl, event, event_data);
		break;
	}
}

/*---------------------------------------------------------------------------*/
/* xio_observable_unreg_all_observers					     */
/*---------------------------------------------------------------------------*/
void xio_observable_unreg_all_observers(struct xio_observable *observable)
{
	struct xio_observer_node *observer_node, *tmp_observer_node;

	list_for_each_entry_safe(observer_node, tmp_observer_node,
				 &observable->observers_list,
				 observers_list_node) {
		list_del(&observer_node->observers_list_node);
		kfree(observer_node);
	}
	observable->observer_node = NULL;
}

