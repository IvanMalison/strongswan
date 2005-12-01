/**
 * @file sa_payload.c
 * 
 * @brief Implementation of sa_payload_t.
 * 
 */

/*
 * Copyright (C) 2005 Jan Hutter, Martin Willi
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
 
/* offsetof macro */
#include <stddef.h>

#include "sa_payload.h"

#include <encoding/payloads/encodings.h>
#include <utils/allocator.h>
#include <utils/linked_list.h>


typedef struct private_sa_payload_t private_sa_payload_t;

/**
 * Private data of an sa_payload_t object.
 * 
 */
struct private_sa_payload_t {
	/**
	 * Public sa_payload_t interface.
	 */
	sa_payload_t public;
	
	/**
	 * Next payload type.
	 */
	u_int8_t  next_payload;

	/**
	 * Critical flag.
	 */
	bool critical;
	
	/**
	 * Length of this payload.
	 */
	u_int16_t payload_length;
	
	/**
	 * Proposals in this payload are stored in a linked_list_t.
	 */
	linked_list_t * proposals;
	
	/**
	 * @brief Computes the length of this payload.
	 *
	 * @param this 	calling private_sa_payload_t object
	 */
	void (*compute_length) (private_sa_payload_t *this);
};

/**
 * Encoding rules to parse or generate a IKEv2-SA Payload
 * 
 * The defined offsets are the positions in a object of type 
 * private_sa_payload_t.
 * 
 */
encoding_rule_t sa_payload_encodings[] = {
 	/* 1 Byte next payload type, stored in the field next_payload */
	{ U_INT_8,		offsetof(private_sa_payload_t, next_payload) 			},
	/* the critical bit */
	{ FLAG,			offsetof(private_sa_payload_t, critical) 				},	
 	/* 7 Bit reserved bits, nowhere stored */
	{ RESERVED_BIT,	0 														}, 
	{ RESERVED_BIT,	0 														}, 
	{ RESERVED_BIT,	0 														}, 
	{ RESERVED_BIT,	0 														}, 
	{ RESERVED_BIT,	0 														}, 
	{ RESERVED_BIT,	0 														}, 
	{ RESERVED_BIT,	0 														}, 
	/* Length of the whole SA payload*/
	{ PAYLOAD_LENGTH,		offsetof(private_sa_payload_t, payload_length) 	},	
	/* Proposals are stored in a proposal substructure, 
	   offset points to a linked_list_t pointer */
	{ PROPOSALS,		offsetof(private_sa_payload_t, proposals) 				}
};

/*
                           1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      ! Next Payload  !C!  RESERVED   !         Payload Length        !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !                                                               !
      ~                          <Proposals>                          ~
      !                                                               !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

/**
 * Implementation of payload_t.verify.
 */
static status_t verify(private_sa_payload_t *this)
{
	int proposal_number = 1;
	status_t status = SUCCESS;
	iterator_t *iterator;
	bool first = TRUE;
	
	if (this->critical)
	{
		/* critical bit set! */
		return FAILED;
	}

	/* check proposal numbering */		
	iterator = this->proposals->create_iterator(this->proposals,TRUE);
	
	while(iterator->has_next(iterator))
	{
		proposal_substructure_t *current_proposal;
		iterator->current(iterator,(void **)&current_proposal);
		if (current_proposal->get_proposal_number(current_proposal) > proposal_number)
		{
			if (first) 
			{
				/* first number must be 1 */
				status = FAILED;
				break;
			}
			
			if (current_proposal->get_proposal_number(current_proposal) != (proposal_number + 1))
			{
				/* must be only one more then previous proposal */
				status = FAILED;
				break;
			}
		}
		else if (current_proposal->get_proposal_number(current_proposal) < proposal_number)
		{
			iterator->destroy(iterator);
			/* must not be smaller then proceeding one */
			status = FAILED;
			break;
		}
		
		status = current_proposal->payload_interface.verify(&(current_proposal->payload_interface));
		if (status != SUCCESS)
		{
			break;
		}
		first = FALSE;
	}
	
	iterator->destroy(iterator);
	return status;
}


/**
 * Implementation of payload_t.destroy and sa_payload_t.destroy.
 */
static status_t destroy(private_sa_payload_t *this)
{
	/* all proposals are getting destroyed */ 
	while (this->proposals->get_count(this->proposals) > 0)
	{
		proposal_substructure_t *current_proposal;
		this->proposals->remove_last(this->proposals,(void **)&current_proposal);
		current_proposal->destroy(current_proposal);
	}
	this->proposals->destroy(this->proposals);
	
	allocator_free(this);
	
	return SUCCESS;
}

/**
 * Implementation of payload_t.get_encoding_rules.
 */
static void get_encoding_rules(private_sa_payload_t *this, encoding_rule_t **rules, size_t *rule_count)
{
	*rules = sa_payload_encodings;
	*rule_count = sizeof(sa_payload_encodings) / sizeof(encoding_rule_t);
}

/**
 * Implementation of payload_t.get_type.
 */
static payload_type_t get_type(private_sa_payload_t *this)
{
	return SECURITY_ASSOCIATION;
}

/**
 * Implementation of payload_t.get_next_type.
 */
static payload_type_t get_next_type(private_sa_payload_t *this)
{
	return (this->next_payload);
}

/**
 * Implementation of payload_t.set_next_type.
 */
static void set_next_type(private_sa_payload_t *this,payload_type_t type)
{
	this->next_payload = type;
}

/**
 * Implementation of payload_t.get_length.
 */
static size_t get_length(private_sa_payload_t *this)
{
	this->compute_length(this);
	return this->payload_length;
}

/**
 * Implementation of sa_payload_t.create_proposal_substructure_iterator.
 */
static iterator_t *create_proposal_substructure_iterator (private_sa_payload_t *this,bool forward)
{
	return this->proposals->create_iterator(this->proposals,forward);
}

/**
 * Implementation of sa_payload_t.add_proposal_substructure.
 */
static void add_proposal_substructure (private_sa_payload_t *this,proposal_substructure_t *proposal)
{
	status_t status;
	if (this->proposals->get_count(this->proposals) > 0)
	{
		proposal_substructure_t *last_proposal;
		status = this->proposals->get_last(this->proposals,(void **) &last_proposal);
		/* last transform is now not anymore last one */
		last_proposal->set_is_last_proposal(last_proposal,FALSE);
	}
	proposal->set_is_last_proposal(proposal,TRUE);
	
	this->proposals->insert_last(this->proposals,(void *) proposal);
	this->compute_length(this);
}

/**
 * Implementation of sa_payload_t.get_ike_proposals.
 */
static status_t get_ike_proposals (private_sa_payload_t *this,ike_proposal_t ** proposals, size_t *proposal_count)
{
	int found_ike_proposals = 0;
	int current_proposal_number = 0;
	iterator_t *iterator;
	ike_proposal_t *tmp_proposals;
	
		
	iterator = this->proposals->create_iterator(this->proposals,TRUE);
	
	/* first find out the number of ike proposals and check their number of transforms and 
	 * if the SPI is empty!*/
	while (iterator->has_next(iterator))
	{
		proposal_substructure_t *current_proposal;
		iterator->current(iterator,(void **)&(current_proposal));
		if (current_proposal->get_protocol_id(current_proposal) == IKE)
		{
			/* a ike proposal consists of 4 transforms and an empty spi*/
			if ((current_proposal->get_transform_count(current_proposal) != 4) ||
			    (current_proposal->get_spi_size(current_proposal) != 0))
		    {
		    	iterator->destroy(iterator);
		    	return FAILED;
		    }
			
			found_ike_proposals++;
		}
	}
	iterator->reset(iterator);
	
	if (found_ike_proposals == 0)
	{
		iterator->destroy(iterator);
		return NOT_FOUND;
	}
	
	/* allocate memory to hold each proposal as ike_proposal_t */
	
	tmp_proposals = allocator_alloc(found_ike_proposals * sizeof(ike_proposal_t));
	
	/* create from each proposal_substructure a ike_proposal_t data area*/
	while (iterator->has_next(iterator))
	{
		proposal_substructure_t *current_proposal;
		iterator->current(iterator,(void **)&(current_proposal));
		if (current_proposal->get_protocol_id(current_proposal) == IKE)
		{
			bool encryption_algorithm_found = FALSE;
			bool integrity_algorithm_found = FALSE;
			bool pseudo_random_function_found = FALSE;
			bool diffie_hellman_group_found = FALSE;
			status_t status;
			iterator_t *transforms;
			
			transforms = current_proposal->create_transform_substructure_iterator(current_proposal,TRUE);
			while (transforms->has_next(transforms))
			{
				transform_substructure_t *current_transform;
				transforms->current(transforms,(void **)&(current_transform));
				
				switch (current_transform->get_transform_type(current_transform))
				{
					case ENCRYPTION_ALGORITHM:
					{
						tmp_proposals[current_proposal_number].encryption_algorithm = current_transform->get_transform_id(current_transform);
						status = current_transform->get_key_length(current_transform,&(tmp_proposals[current_proposal_number].encryption_algorithm_key_length));
						if (status == SUCCESS)
						{
							encryption_algorithm_found = TRUE;
						}
						break;
					}
					case INTEGRITY_ALGORITHM:
					{
						tmp_proposals[current_proposal_number].integrity_algorithm = current_transform->get_transform_id(current_transform);
						status = current_transform->get_key_length(current_transform,&(tmp_proposals[current_proposal_number].integrity_algorithm_key_length));
						if (status == SUCCESS)
						{
							integrity_algorithm_found = TRUE;
						}
						break;
					}
					case PSEUDO_RANDOM_FUNCTION:
					{
						tmp_proposals[current_proposal_number].pseudo_random_function = current_transform->get_transform_id(current_transform);
						status = current_transform->get_key_length(current_transform,&(tmp_proposals[current_proposal_number].pseudo_random_function_key_length));
						if (status == SUCCESS)
						{
							pseudo_random_function_found = TRUE;
						}
						break;
					}
					case DIFFIE_HELLMAN_GROUP:
					{
						tmp_proposals[current_proposal_number].diffie_hellman_group = current_transform->get_transform_id(current_transform);
						diffie_hellman_group_found = TRUE;
						break;
					}
					default:
					{
						/* not a transform of an ike proposal. Break here */
						break;
					}
				}
				
			}

			transforms->destroy(transforms);
			
			if ((!encryption_algorithm_found) ||
				(!integrity_algorithm_found) ||
				(!pseudo_random_function_found) ||
				(!diffie_hellman_group_found))
			{
				/* one of needed transforms could not be found */
				iterator->reset(iterator);
				allocator_free(tmp_proposals);
				return FAILED;
			}
			
			current_proposal_number++;
		}
	}

	iterator->destroy(iterator);	
	
	*proposals = tmp_proposals;
	*proposal_count = found_ike_proposals;

	return SUCCESS;
}

/**
 * Implementation of private_sa_payload_t.compute_length.
 */
static void compute_length (private_sa_payload_t *this)
{
	iterator_t *iterator;
	size_t length = SA_PAYLOAD_HEADER_LENGTH;
	iterator = this->proposals->create_iterator(this->proposals,TRUE);
	while (iterator->has_next(iterator))
	{
		payload_t *current_proposal;
		iterator->current(iterator,(void **) &current_proposal);
		length += current_proposal->get_length(current_proposal);
	}
	iterator->destroy(iterator);
	
	this->payload_length = length;
}

/*
 * Described in header.
 */
sa_payload_t *sa_payload_create()
{
	private_sa_payload_t *this = allocator_alloc_thing(private_sa_payload_t);
	
	/* public interface */
	this->public.payload_interface.verify = (status_t (*) (payload_t *))verify;
	this->public.payload_interface.get_encoding_rules = (void (*) (payload_t *, encoding_rule_t **, size_t *) ) get_encoding_rules;
	this->public.payload_interface.get_length = (size_t (*) (payload_t *)) get_length;
	this->public.payload_interface.get_next_type = (payload_type_t (*) (payload_t *)) get_next_type;
	this->public.payload_interface.set_next_type = (void (*) (payload_t *,payload_type_t)) set_next_type;
	this->public.payload_interface.get_type = (payload_type_t (*) (payload_t *)) get_type;
	this->public.payload_interface.destroy = (void (*) (payload_t *))destroy;
	
	/* public functions */
	this->public.create_proposal_substructure_iterator = (iterator_t* (*) (sa_payload_t *,bool)) create_proposal_substructure_iterator;
	this->public.add_proposal_substructure = (void (*) (sa_payload_t *,proposal_substructure_t *)) add_proposal_substructure;
	this->public.get_ike_proposals = (status_t (*) (sa_payload_t *, ike_proposal_t **, size_t *)) get_ike_proposals;
	this->public.destroy = (void (*) (sa_payload_t *)) destroy;
	
	/* private functions */
	this->compute_length = compute_length;
	
	/* set default values of the fields */
	this->critical = SA_PAYLOAD_CRITICAL_FLAG;
	this->next_payload = NO_PAYLOAD;
	this->payload_length = SA_PAYLOAD_HEADER_LENGTH;

	this->proposals = linked_list_create();
	return (&(this->public));
}

/*
 * Described in header.
 */
sa_payload_t *sa_payload_create_from_ike_proposals(ike_proposal_t *proposals, size_t proposal_count)
{
	int i;
	sa_payload_t *sa_payload= sa_payload_create();
	
	for (i = 0; i < proposal_count; i++)
	{
		proposal_substructure_t *proposal_substructure;
		transform_substructure_t *encryption_algorithm;
		transform_substructure_t *integrity_algorithm;
		transform_substructure_t *pseudo_random_function;
		transform_substructure_t *diffie_hellman_group;
		
		/* create proposal substructure */
		proposal_substructure = proposal_substructure_create();
		proposal_substructure->set_protocol_id(proposal_substructure,IKE);
		proposal_substructure->set_proposal_number(proposal_substructure,(i + 1));

		/* create transform substructures to hold each specific transform for an ike proposal */
		encryption_algorithm = transform_substructure_create_type(ENCRYPTION_ALGORITHM,proposals[i].encryption_algorithm,proposals[i].encryption_algorithm_key_length);
		proposal_substructure->add_transform_substructure(proposal_substructure,encryption_algorithm);
		
		pseudo_random_function = transform_substructure_create_type(PSEUDO_RANDOM_FUNCTION,proposals[i].pseudo_random_function,proposals[i].pseudo_random_function_key_length);
		proposal_substructure->add_transform_substructure(proposal_substructure,pseudo_random_function);

		integrity_algorithm = transform_substructure_create_type(INTEGRITY_ALGORITHM,proposals[i].integrity_algorithm,proposals[i].integrity_algorithm_key_length);
		proposal_substructure->add_transform_substructure(proposal_substructure,integrity_algorithm);

		diffie_hellman_group = transform_substructure_create_type(DIFFIE_HELLMAN_GROUP,proposals[i].diffie_hellman_group,0);
		proposal_substructure->add_transform_substructure(proposal_substructure,diffie_hellman_group);
		
		/* add proposal to sa payload */
		sa_payload->add_proposal_substructure(sa_payload,proposal_substructure);
	}
	
	return sa_payload;
}
