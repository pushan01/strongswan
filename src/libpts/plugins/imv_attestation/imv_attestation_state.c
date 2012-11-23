/*
 * Copyright (C) 2011-2012 Sansar Choinyambuu, Andreas Steffen
 * HSR Hochschule fuer Technik Rapperswil
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

#include "imv_attestation_state.h"

#include <libpts.h>

#include <imv/imv_lang_string.h>
#include "imv/imv_reason_string.h"

#include <utils/lexparser.h>
#include <collections/linked_list.h>
#include <utils/debug.h>

typedef struct private_imv_attestation_state_t private_imv_attestation_state_t;
typedef struct file_meas_request_t file_meas_request_t;
typedef struct func_comp_t func_comp_t;

/**
 * Private data of an imv_attestation_state_t object.
 */
struct private_imv_attestation_state_t {

	/**
	 * Public members of imv_attestation_state_t
	 */
	imv_attestation_state_t public;

	/**
	 * TNCCS connection ID
	 */
	TNC_ConnectionID connection_id;

	/**
	 * TNCCS connection state
	 */
	TNC_ConnectionState state;

	/**
	 * Does the TNCCS connection support long message types?
	 */
	bool has_long;

	/**
	 * Does the TNCCS connection support exclusive delivery?
	 */
	bool has_excl;

	/**
	 * Maximum PA-TNC message size for this TNCCS connection
	 */
	u_int32_t max_msg_len;

	/**
	 * IMV Attestation handshake state
	 */
	imv_attestation_handshake_state_t handshake_state;

	/**
	 * IMV action recommendation
	 */
	TNC_IMV_Action_Recommendation rec;

	/**
	 * IMV evaluation result
	 */
	TNC_IMV_Evaluation_Result eval;

	/**
	 * File Measurement Request counter
	 */
	u_int16_t file_meas_request_counter;

	/**
	 * List of PTS File/Directory Measurement requests
	 */
	linked_list_t *file_meas_requests;

	/**
	 * List of Functional Components
	 */
	linked_list_t *components;

	/**
	 * PTS object
	 */
	pts_t *pts;

	/**
	 * Measurement error
	 */
	bool measurement_error;

	/**
	 * TNC Reason String
	 */
	imv_reason_string_t *reason_string;

};

/**
 * PTS File/Directory Measurement request entry
 */
struct file_meas_request_t {
	u_int16_t id;
	int file_id;
	bool is_dir;
};

/**
 * PTS Functional Component entry
 */
struct func_comp_t {
	pts_component_t *comp;
	u_int8_t qualifier;
};

/**
 * Frees a func_comp_t object
 */
static void free_func_comp(func_comp_t *this)
{
	this->comp->destroy(this->comp);
	free(this);
}

/**
 * Supported languages
 */
static char* languages[] = { "en", "mn", "de" };

/**
 * Table of reason strings
 */
static imv_lang_string_t reasons[] = {
	{ "en", "IMV Attestation: Incorrect/pending file measurement/component"
			" evidence or invalid TPM Quote signature received" },
	{ "mn", "IMV Attestation:  Буруу/хүлээгдэж байгаа файл/компонент хэмжилт "
			"эсвэл буруу TPM Quote гарын үсэг" },
	{ "de", "IMV Attestation: Falsche/Fehlende Dateimessung/Komponenten Beweis "
			"oder ungültige TPM Quote Unterschrift ist erhalten" },
	{ NULL, NULL }
};

METHOD(imv_state_t, get_connection_id, TNC_ConnectionID,
	private_imv_attestation_state_t *this)
{
	return this->connection_id;
}

METHOD(imv_state_t, has_long, bool,
	private_imv_attestation_state_t *this)
{
	return this->has_long;
}

METHOD(imv_state_t, has_excl, bool,
	private_imv_attestation_state_t *this)
{
	return this->has_excl;
}

METHOD(imv_state_t, set_flags, void,
	private_imv_attestation_state_t *this, bool has_long, bool has_excl)
{
	this->has_long = has_long;
	this->has_excl = has_excl;
}

METHOD(imv_state_t, set_max_msg_len, void,
	private_imv_attestation_state_t *this, u_int32_t max_msg_len)
{
	this->max_msg_len = max_msg_len;
}

METHOD(imv_state_t, get_max_msg_len, u_int32_t,
	private_imv_attestation_state_t *this)
{
	return this->max_msg_len;
}

METHOD(imv_state_t, change_state, void,
	private_imv_attestation_state_t *this, TNC_ConnectionState new_state)
{
	this->state = new_state;
}

METHOD(imv_state_t, get_recommendation, void,
	private_imv_attestation_state_t *this, TNC_IMV_Action_Recommendation *rec,
									TNC_IMV_Evaluation_Result *eval)
{
	*rec = this->rec;
	*eval = this->eval;
}

METHOD(imv_state_t, set_recommendation, void,
	private_imv_attestation_state_t *this, TNC_IMV_Action_Recommendation rec,
									TNC_IMV_Evaluation_Result eval)
{
	this->rec = rec;
	this->eval = eval;
}

METHOD(imv_state_t, get_reason_string, bool,
	private_imv_attestation_state_t *this, enumerator_t *language_enumerator,
	chunk_t *reason_string, char **reason_language)
{
	*reason_language = imv_lang_string_select_lang(language_enumerator,
											  languages, countof(languages));

	/* Instantiate a TNC Reason String object */
	DESTROY_IF(this->reason_string);
	this->reason_string = imv_reason_string_create(*reason_language);
	this->reason_string->add_reason(this->reason_string, reasons);
	*reason_string = this->reason_string->get_encoding(this->reason_string);

	return TRUE;
}

METHOD(imv_state_t, get_remediation_instructions, bool,
	private_imv_attestation_state_t *this, enumerator_t *language_enumerator,
	chunk_t *string, char **lang_code, char **uri)
{
	return FALSE;
}

METHOD(imv_state_t, destroy, void,
	private_imv_attestation_state_t *this)
{
	DESTROY_IF(this->reason_string);
	this->file_meas_requests->destroy_function(this->file_meas_requests, free);
	this->components->destroy_function(this->components, (void *)free_func_comp);
	this->pts->destroy(this->pts);
	free(this);
}

METHOD(imv_attestation_state_t, get_handshake_state,
	   imv_attestation_handshake_state_t, private_imv_attestation_state_t *this)
{
	return this->handshake_state;
}

METHOD(imv_attestation_state_t, set_handshake_state, void,
	private_imv_attestation_state_t *this,
	imv_attestation_handshake_state_t new_state)
{
	this->handshake_state = new_state;
}

METHOD(imv_attestation_state_t, get_pts, pts_t*,
	private_imv_attestation_state_t *this)
{
	return this->pts;
}

METHOD(imv_attestation_state_t, add_file_meas_request, u_int16_t,
	private_imv_attestation_state_t *this, int file_id, bool is_dir)
{
	file_meas_request_t *request;

	request = malloc_thing(file_meas_request_t);
	request->id = ++this->file_meas_request_counter;
	request->file_id = file_id;
	request->is_dir = is_dir;
	this->file_meas_requests->insert_last(this->file_meas_requests, request);

	return this->file_meas_request_counter;
}

METHOD(imv_attestation_state_t, check_off_file_meas_request, bool,
	private_imv_attestation_state_t *this, u_int16_t id, int *file_id,
	bool* is_dir)
{
	enumerator_t *enumerator;
	file_meas_request_t *request;
	bool found = FALSE;

	enumerator = this->file_meas_requests->create_enumerator(this->file_meas_requests);
	while (enumerator->enumerate(enumerator, &request))
	{
		if (request->id == id)
		{
			found = TRUE;
			*file_id = request->file_id;
			*is_dir = request->is_dir;
			this->file_meas_requests->remove_at(this->file_meas_requests, enumerator);
			free(request);
			break;
		}
	}
	enumerator->destroy(enumerator);
	return found;
}

METHOD(imv_attestation_state_t, get_file_meas_request_count, int,
	private_imv_attestation_state_t *this)
{
	return this->file_meas_requests->get_count(this->file_meas_requests);
}

METHOD(imv_attestation_state_t, create_component, pts_component_t*,
	private_imv_attestation_state_t *this, pts_comp_func_name_t *name,
	u_int32_t depth, pts_database_t *pts_db)
{
	enumerator_t *enumerator;
	func_comp_t *entry, *new_entry;
	pts_component_t *component;
	bool found = FALSE;

	enumerator = this->components->create_enumerator(this->components);
	while (enumerator->enumerate(enumerator, &entry))
	{
		if (name->equals(name, entry->comp->get_comp_func_name(entry->comp)))
		{
			found = TRUE;
			break;
		}
	}
	enumerator->destroy(enumerator);

	if (found)
	{
		if (name->get_qualifier(name) == entry->qualifier)
		{
			/* duplicate entry */
			return NULL;
		}
		new_entry = malloc_thing(func_comp_t);
		new_entry->qualifier = name->get_qualifier(name);
		new_entry->comp = entry->comp->get_ref(entry->comp);
		this->components->insert_last(this->components, new_entry);
		return entry->comp;
	}
	else
	{
		component = pts_components->create(pts_components, name, depth, pts_db);
		if (!component)
		{
			/* unsupported component */
			return NULL;
		}
		new_entry = malloc_thing(func_comp_t);
		new_entry->qualifier = name->get_qualifier(name);
		new_entry->comp = component;
		this->components->insert_last(this->components, new_entry);
		return component;
	}
}

METHOD(imv_attestation_state_t, get_component, pts_component_t*,
	private_imv_attestation_state_t *this, pts_comp_func_name_t *name)
{
	enumerator_t *enumerator;
	func_comp_t *entry;
	pts_component_t *found = NULL;

	enumerator = this->components->create_enumerator(this->components);
	while (enumerator->enumerate(enumerator, &entry))
	{
		if (name->equals(name, entry->comp->get_comp_func_name(entry->comp)) &&
			name->get_qualifier(name) == entry->qualifier)
		{
			found = entry->comp;
			break;
		}
	}
	enumerator->destroy(enumerator);
	return found;
}

METHOD(imv_attestation_state_t, get_measurement_error, bool,
	private_imv_attestation_state_t *this)
{
	return this->measurement_error;
}

METHOD(imv_attestation_state_t, set_measurement_error, void,
	private_imv_attestation_state_t *this)
{
	this->measurement_error = TRUE;
}

METHOD(imv_attestation_state_t, finalize_components, void,
	private_imv_attestation_state_t *this)
{
	func_comp_t *entry;

	while (this->components->remove_last(this->components,
										(void**)&entry) == SUCCESS)
	{
		if (!entry->comp->finalize(entry->comp, entry->qualifier))
		{
			_set_measurement_error(this);
		}
		free_func_comp(entry);
	}
}

METHOD(imv_attestation_state_t, components_finalized, bool,
	private_imv_attestation_state_t *this)
{
	return this->components->get_count(this->components) == 0;
}

/**
 * Described in header.
 */
imv_state_t *imv_attestation_state_create(TNC_ConnectionID connection_id)
{
	private_imv_attestation_state_t *this;

	INIT(this,
		.public = {
			.interface = {
				.get_connection_id = _get_connection_id,
				.has_long = _has_long,
				.has_excl = _has_excl,
				.set_flags = _set_flags,
				.set_max_msg_len = _set_max_msg_len,
				.get_max_msg_len = _get_max_msg_len,
				.change_state = _change_state,
				.get_recommendation = _get_recommendation,
				.set_recommendation = _set_recommendation,
				.get_reason_string = _get_reason_string,
				.get_remediation_instructions = _get_remediation_instructions,
				.destroy = _destroy,
			},
			.get_handshake_state = _get_handshake_state,
			.set_handshake_state = _set_handshake_state,
			.get_pts = _get_pts,
			.add_file_meas_request = _add_file_meas_request,
			.check_off_file_meas_request = _check_off_file_meas_request,
			.get_file_meas_request_count = _get_file_meas_request_count,
			.create_component = _create_component,
			.get_component = _get_component,
			.finalize_components = _finalize_components,
			.components_finalized = _components_finalized,
			.get_measurement_error = _get_measurement_error,
			.set_measurement_error = _set_measurement_error,
		},
		.connection_id = connection_id,
		.state = TNC_CONNECTION_STATE_CREATE,
		.handshake_state = IMV_ATTESTATION_STATE_INIT,
		.rec = TNC_IMV_ACTION_RECOMMENDATION_NO_RECOMMENDATION,
		.eval = TNC_IMV_EVALUATION_RESULT_DONT_KNOW,
		.file_meas_requests = linked_list_create(),
		.components = linked_list_create(),
		.pts = pts_create(FALSE),
	);

	return &this->public.interface;
}
