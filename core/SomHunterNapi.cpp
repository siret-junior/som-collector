
/* This file is part of SOMHunter.
 *
 * Copyright (C) 2020 František Mejzlík <frankmejzlik@gmail.com>
 *                    Mirek Kratochvil <exa.exa@gmail.com>
 *                    Patrik Veselý <prtrikvesely@gmail.com>
 *
 * SOMHunter is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 2 of the License, or (at your option)
 * any later version.
 *
 * SOMHunter is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SOMHunter. If not, see <https://www.gnu.org/licenses/>.
 */
#include "common.h"

#include "SomHunterNapi.h"

#include <stdexcept>

Napi::FunctionReference SomHunterNapi::constructor;

Napi::Object
SomHunterNapi::Init(Napi::Env env, Napi::Object exports)
{
	Napi::HandleScope scope(env);

	Napi::Function func = DefineClass(
	  env,
	  "SomHunterNapi",
	  { InstanceMethod("getDisplay", &SomHunterNapi::get_display),
	    InstanceMethod("getTarget", &SomHunterNapi::get_target_image),
	    InstanceMethod("getPreviousDisplay",
	                   &SomHunterNapi::get_previous_display),
	    InstanceMethod("addLikes", &SomHunterNapi::add_likes),
	    InstanceMethod("rescore", &SomHunterNapi::rescore),
	    InstanceMethod("resetAll", &SomHunterNapi::reset_all),
	    InstanceMethod("getAvailableDisplay",
	                   &SomHunterNapi::get_available_display),
	    InstanceMethod("removeLikes", &SomHunterNapi::remove_likes),
	    InstanceMethod("autocompleteKeywords",
	                   &SomHunterNapi::autocomplete_keywords),
	    InstanceMethod("isSomReady", &SomHunterNapi::is_som_ready),
	    InstanceMethod("submitToServer", &SomHunterNapi::submit_to_server),
	    InstanceMethod("getLevelInfo", &SomHunterNapi::get_level_info),
	    InstanceMethod("getLastTextQuery",
	                   &SomHunterNapi::get_last_text_query),
	    InstanceMethod("reportIssue", &SomHunterNapi::report_issue) });

	constructor = Napi::Persistent(func);
	constructor.SuppressDestruct();

	exports.Set("SomHunterNapi", func);

	return exports;
}

SomHunterNapi::SomHunterNapi(const Napi::CallbackInfo &info)
  : Napi::ObjectWrap<SomHunterNapi>(info)
{
	debug("API: Instantiating SomHunter...");

	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();
	if (length != 1) {
		Napi::TypeError::New(env, "Wrong number of parameters")
		  .ThrowAsJavaScriptException();
	}

	std::string config_fpth = info[0].As<Napi::String>().Utf8Value();

	// Parse the config
	Config cfg = Config::parse_json_config(config_fpth);
	try {
		debug("API: Creating SomHuntersGuild");
		somhunter = new SomHuntersGuild(cfg);
		debug("API: SomHunter initialized.");
	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}
}
Napi::Value
SomHunterNapi::get_previous_display(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	std::string path_prefix{ info[1].As<Napi::String>().Utf8Value() };

	// Call native method
	PreviousDisplay previous_display;
	try {
		debug("API: CALL get_previous_display");

		auto hunter = somhunter->get(usr);
		if (hunter == nullptr)
			warn("Hunter not found for user " << usr << "!!!");

		previous_display = hunter->get_previous_display();

		debug("API: RETURN \n\t get_previous_display");
	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}
	napi_value result;
	napi_create_object(env, &result);

	// Set "frames"
	{
		napi_value upperKey;
		napi_create_string_utf8(
		  env, "frames", NAPI_AUTO_LENGTH, &upperKey);

		// Create array
		napi_value arr;
		napi_create_array(env, &arr);
		{
			size_t i{ 0_z };
			for (auto it{ previous_display.display.begin() };
			     it != previous_display.display.end();
			     ++it) {

				napi_value obj;
				napi_create_object(env, &obj);
				{
					ImageId ID{ IMAGE_ID_ERR_VAL };
					bool is_liked{ false };
					std::string filename{};

					if ((*it) != nullptr) {
						ID = (*it)->frame_ID;
						is_liked = (*it)->liked;
						filename =
						  path_prefix + (*it)->filename;
					}

					{
						napi_value key;
						napi_create_string_utf8(
						  env,
						  "id",
						  NAPI_AUTO_LENGTH,
						  &key);

						napi_value value;
						if (ID == IMAGE_ID_ERR_VAL) {
							napi_get_null(env,
							              &value);
						} else {
							napi_create_uint32(
							  env,
							  uint32_t(ID),
							  &value);
						}

						napi_set_property(
						  env, obj, key, value);
					}

					{
						napi_value key;
						napi_create_string_utf8(
						  env,
						  "liked",
						  NAPI_AUTO_LENGTH,
						  &key);

						napi_value value;
						napi_get_boolean(
						  env, is_liked, &value);

						napi_set_property(
						  env, obj, key, value);
					}

					{
						napi_value key;
						napi_create_string_utf8(
						  env,
						  "src",
						  NAPI_AUTO_LENGTH,
						  &key);

						napi_value value;
						napi_create_string_utf8(
						  env,
						  filename.c_str(),
						  NAPI_AUTO_LENGTH,
						  &value);

						napi_set_property(
						  env, obj, key, value);
					}
				}
				napi_set_element(env, arr, i, obj);

				++i;
			}
		}

		napi_set_property(env, result, upperKey, arr);
		

		
		napi_value histogramKey;
		napi_create_string_utf8(
		  env, "histogram", NAPI_AUTO_LENGTH, &histogramKey);

		// Create array
		napi_value arr2;
		napi_create_array(env, &arr2);
		{
			size_t i{ 0_z };
			for (auto it{ previous_display.histogram.begin() };
			     it != previous_display.histogram.end();
			     ++it) {

				napi_value obj;
				napi_create_object(env, &obj);
				{
					int histogram{ 0 };

					histogram = (*it);

					{
						napi_value key;
						napi_create_string_utf8(
						  env,
						  "histogram",
						  NAPI_AUTO_LENGTH,
						  &key);

						napi_value value;

						napi_create_double(
						  env, histogram, &value);

						napi_set_property(
						  env, obj, histogramKey, value);
					}
				}
				napi_set_element(env, arr2, i, obj);

				++i;
			}
		}
		napi_set_property(env, result, histogramKey, arr2);



		napi_value upperKey1;
		napi_create_string_utf8(
		  env, "distances", NAPI_AUTO_LENGTH, &upperKey1);

		// Create array
		napi_value arr1;
		napi_create_array(env, &arr1);
		{
			size_t i{ 0_z };
			for (auto it{ previous_display.distances.begin() };
			     it != previous_display.distances.end();
			     ++it) {

				napi_value obj;
				napi_create_object(env, &obj);
				{
					float distance{ 0.0 };

					distance = (*it);

					{
						napi_value key;
						napi_create_string_utf8(
						  env,
						  "distance",
						  NAPI_AUTO_LENGTH,
						  &key);

						napi_value value;

						napi_create_double(
						  env, distance, &value);

						napi_set_property(
						  env, obj, key, value);
					}
				}
				napi_set_element(env, arr1, i, obj);

				++i;
			}
		}
		napi_set_property(env, result, upperKey1, arr1);

		napi_value key;
		napi_create_string_utf8(
		  env, "targetPath", NAPI_AUTO_LENGTH, &key);
		napi_value obj;
		napi_create_object(env, &obj);
		{
			ImageId ID{ IMAGE_ID_ERR_VAL };
			bool is_liked{ false };
			std::string filename{};

			if (previous_display.target != nullptr) {
				ID = previous_display.target->frame_ID;
				is_liked = previous_display.target->liked;
				filename = path_prefix +
				           previous_display.target->filename;
			}

			{
				napi_value key;
				napi_create_string_utf8(
				  env, "id", NAPI_AUTO_LENGTH, &key);

				napi_value value;
				if (ID == IMAGE_ID_ERR_VAL) {
					napi_get_null(env, &value);
				} else {
					napi_create_uint32(
					  env, uint32_t(ID), &value);
				}

				napi_set_property(env, obj, key, value);
			}

			{
				napi_value key;
				napi_create_string_utf8(
				  env, "liked", NAPI_AUTO_LENGTH, &key);

				napi_value value;
				napi_get_boolean(env, is_liked, &value);

				napi_set_property(env, obj, key, value);
			}

			{
				napi_value key;
				napi_create_string_utf8(
				  env, "src", NAPI_AUTO_LENGTH, &key);

				napi_value value;
				napi_create_string_utf8(env,
				                        filename.c_str(),
				                        NAPI_AUTO_LENGTH,
				                        &value);

				napi_set_property(env, obj, key, value);
			}
		}

		napi_set_property(env, result, key, obj);
	}
	return Napi::Object(env, result);
}

Napi::Value
SomHunterNapi::get_display(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();
	if (length > 5) {
		Napi::TypeError::New(env,
		                     "Wrong number of parameters "
		                     "(SomHunterNapi::get_display)")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	DisplayType disp_type{ DisplayType::DTopN };
	ImageId selected_image{ IMAGE_ID_ERR_VAL };
	size_t page_num{ 0 };

	std::string path_prefix{ info[1].As<Napi::String>().Utf8Value() };
	// Get the display type
	std::string display_string{ info[2].As<Napi::String>().Utf8Value() };
	if (display_string == "topn") {
		disp_type = DisplayType::DTopN;
		page_num = info[3].As<Napi::Number>().Uint32Value();

	} else if (display_string == "som") {
		disp_type = DisplayType::DSom;
	} else if (display_string == "detail") {
		disp_type = DisplayType::DVideoDetail;
		selected_image = info[4].As<Napi::Number>().Uint32Value();
	} else if (display_string == "topknn") {
		disp_type = DisplayType::DTopKNN;
		selected_image = info[4].As<Napi::Number>().Uint32Value();
	}

	// Call native method
	FramePointerRange display_frames;
	try {
		debug("API: CALL \n\t get_display\n\t\tdisp_type = "
		      << int(disp_type) << std::endl
		      << "n\t\t selected_image = " << selected_image
		      << std::endl
		      << "n\t\t page_num = " << page_num);

		auto hunter = somhunter->get(usr);
		if (hunter == nullptr)
			warn("Hunter not found for user " << usr << "!!!");

		display_frames =
		  hunter->get_display(disp_type, selected_image, page_num);

		debug("API: RETURN \n\t get_display\n\t\tframes.size() = "
		      << display_frames.size());
	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	napi_value result;
	napi_create_object(env, &result);

	// Set "page"
	{
		napi_value key;
		napi_create_string_utf8(env, "page", NAPI_AUTO_LENGTH, &key);

		napi_value value;
		napi_create_uint32(env, uint32_t(page_num), &value);

		napi_set_property(env, result, key, value);
	}

	// Set "frames"
	{
		napi_value upperKey;
		napi_create_string_utf8(
		  env, "frames", NAPI_AUTO_LENGTH, &upperKey);

		// Create array
		napi_value arr;
		napi_create_array(env, &arr);
		{
			size_t i{ 0_z };
			for (auto it{ display_frames.begin() };
			     it != display_frames.end();
			     ++it) {

				napi_value obj;
				napi_create_object(env, &obj);
				{
					ImageId ID{ IMAGE_ID_ERR_VAL };
					bool is_liked{ false };
					std::string filename{};

					if ((*it) != nullptr) {
						ID = (*it)->frame_ID;
						is_liked = (*it)->liked;
						filename =
						  path_prefix + (*it)->filename;
					}

					{
						napi_value key;
						napi_create_string_utf8(
						  env,
						  "id",
						  NAPI_AUTO_LENGTH,
						  &key);

						napi_value value;
						if (ID == IMAGE_ID_ERR_VAL) {
							napi_get_null(env,
							              &value);
						} else {
							napi_create_uint32(
							  env,
							  uint32_t(ID),
							  &value);
						}

						napi_set_property(
						  env, obj, key, value);
					}

					{
						napi_value key;
						napi_create_string_utf8(
						  env,
						  "liked",
						  NAPI_AUTO_LENGTH,
						  &key);

						napi_value value;
						napi_get_boolean(
						  env, is_liked, &value);

						napi_set_property(
						  env, obj, key, value);
					}

					{
						napi_value key;
						napi_create_string_utf8(
						  env,
						  "src",
						  NAPI_AUTO_LENGTH,
						  &key);

						napi_value value;
						napi_create_string_utf8(
						  env,
						  filename.c_str(),
						  NAPI_AUTO_LENGTH,
						  &value);

						napi_set_property(
						  env, obj, key, value);
					}
				}
				napi_set_element(env, arr, i, obj);

				++i;
			}
		}

		napi_set_property(env, result, upperKey, arr);
	}

	return Napi::Object(env, result);
}

Napi::Value
SomHunterNapi::get_target_image(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	size_t length = info.Length();

	if (length != 1) {
		Napi::TypeError::New(env,
		                     "Wrong number of parameters "
		                     "(SomHunterNapi::get_display)")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	VideoFramePointer target;
	try {
		debug("API: CALL \n\t get_target_image" << std::endl);

		target = somhunter->get(usr)->get_target_image();
	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	napi_value result_dict;
	napi_create_object(env, &result_dict);

	{
		napi_value key;
		napi_create_string_utf8(
		  env, "targetPath", NAPI_AUTO_LENGTH, &key);
		napi_value value;
		if (target->frame_ID != PREPARE_IMAGE_ID) {
			napi_create_string_utf8(
			env, target->filename.c_str(), NAPI_AUTO_LENGTH, &value);
		} else {
			napi_create_string_utf8(
			env, "nothing.jpg", NAPI_AUTO_LENGTH, &value);
		}

		napi_set_property(env, result_dict, key, value);
	}

	return Napi::Object(env, result_dict);
}

Napi::Value
SomHunterNapi::add_likes(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();

	if (length != 2) {
		Napi::TypeError::New(env, "Wrong number of parameters")
		  .ThrowAsJavaScriptException();
	}

	std::vector<ImageId> fr_IDs;

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	Napi::Array arr = info[1].As<Napi::Array>();
	for (size_t i{ 0 }; i < arr.Length(); ++i) {
		Napi::Value val = arr[i];

		size_t fr_ID{ val.As<Napi::Number>().Uint32Value() };
		fr_IDs.emplace_back(fr_ID);
	}

	try {
		debug("API: CALL \n\t add_likes\n\t\fr_IDs.size() = "
		      << fr_IDs.size() << std::endl);

		somhunter->get(usr)->add_likes(fr_IDs);
	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	return Napi::Object{};
}

Napi::Value
SomHunterNapi::rescore(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();

	if (length != 2) {
		Napi::TypeError::New(
		  env, "Wrong number of parameters: SomHunterNapi::rescore")
		  .ThrowAsJavaScriptException();
	}
	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	std::string query{ info[1].As<Napi::String>().Utf8Value() };

	SearchState state;
	try {
		debug("API: CALL \n\t rescore\n\t\t query =  " << query
		                                               << std::endl);

		state = somhunter->get(usr)->rescore(query);

	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	napi_value result;
	napi_create_object(env, &result);

	// Set "nextDisplay"
	{
		napi_value key;
		napi_create_string_utf8(env, "nextDisplay", NAPI_AUTO_LENGTH, &key);

		napi_value value;
		if (state.nextDisplay == DisplayType::DSom)
			napi_create_string_utf8(env, "som", 3, &value);
		else
			napi_create_string_utf8(env, "topn", 4, &value);

		napi_set_property(env, result, key, value);
	}

	// Set "reformulations"
	{
		napi_value key;
		napi_create_string_utf8(env, "reformulations", NAPI_AUTO_LENGTH, &key);

		napi_value value;
		napi_create_uint32(env, uint32_t(state.reformulations), &value);

		napi_set_property(env, result, key, value);
	}

	// Set "feedbacks"
	{
		napi_value key;
		napi_create_string_utf8(env, "feedbacks", NAPI_AUTO_LENGTH, &key);

		napi_value value;
		napi_create_uint32(env, uint32_t(state.feedbacks), &value);

		napi_set_property(env, result, key, value);
	}

	return Napi::Object(env, result);
}

Napi::Value
SomHunterNapi::get_available_display(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();
	if (length != 1) {
		Napi::TypeError::New(env,
		                     "Wrong number of parameters "
		                     "(SomHunterNapi::get_available_display)")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	DisplayType disp = DisplayType::DNull;
	try {
		debug("API: CALL \n\t get_available_display()");

		disp = somhunter->get(usr)->get_available_display();

	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	napi_value result;
	if (disp == DisplayType::DSom)
		napi_create_string_utf8(env, "som", 3, &result);
	else
		napi_create_string_utf8(env, "topn", 4, &result);

	return Napi::Object(env, result);
}

Napi::Value
SomHunterNapi::reset_all(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();
	if (length != 1) {
		Napi::TypeError::New(env,
		                     "Wrong number of parameters "
		                     "(SomHunterNapi::reset_all)")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	DisplayType disp = DisplayType::DNull;
	try {
		debug("API: CALL \n\t reset_all()");

		disp = somhunter->get(usr)->reset_search_session();

	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	napi_value result;
	if (disp == DisplayType::DSom)
		napi_create_string_utf8(env, "som", 3, &result);
	else
		napi_create_string_utf8(env, "topn", 4, &result);

	return Napi::Object(env, result);
}

Napi::Value
SomHunterNapi::remove_likes(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();

	if (length != 2) {
		Napi::TypeError::New(env, "Wrong number of parameters")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	std::vector<ImageId> fr_IDs;
	Napi::Array arr = info[1].As<Napi::Array>();
	for (size_t i{ 0 }; i < arr.Length(); ++i) {
		Napi::Value val = arr[i];

		size_t fr_ID{ val.As<Napi::Number>().Uint32Value() };
		fr_IDs.emplace_back(fr_ID);
	}

	try {
		debug("API: CALL \n\t add_likes\n\t\fr_IDs.size() = "
		      << fr_IDs.size() << std::endl);

		somhunter->get(usr)->add_likes(fr_IDs);
	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	return Napi::Object{};
}

Napi::Value
SomHunterNapi::autocomplete_keywords(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();

	if (length != 4) {
		Napi::TypeError::New(env, "Wrong number of parameters")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	std::string path_prefix{ info[1].As<Napi::String>().Utf8Value() };
	std::string prefix{ info[2].As<Napi::String>().Utf8Value() };
	size_t count{ info[3].As<Napi::Number>().Uint32Value() };

	// Get suggested keywords
	std::vector<const Keyword *> kws;
	try {
		kws = somhunter->get(usr)->autocomplete_keywords(prefix, count);
	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	// Return structure
	napi_value result_arr;
	napi_create_array(env, &result_arr);

	size_t i = 0ULL;
	// Iterate through all results
	for (auto &&p_kw : kws) {

		napi_value single_result_dict;
		napi_create_object(env, &single_result_dict);

		// ID
		{
			napi_value key;
			napi_create_string_utf8(
			  env, "id", NAPI_AUTO_LENGTH, &key);
			napi_value value;
			napi_create_uint32(env, p_kw->kw_ID, &value);

			napi_set_property(env, single_result_dict, key, value);
		}

		// wordString
		{
			napi_value key;
			napi_create_string_utf8(
			  env, "wordString", NAPI_AUTO_LENGTH, &key);
			napi_value value;
			napi_create_string_utf8(
			  env,
			  p_kw->synset_strs.front().c_str(),
			  NAPI_AUTO_LENGTH,
			  &value);

			napi_set_property(env, single_result_dict, key, value);
		}

		// description
		{
			napi_value key;
			napi_create_string_utf8(
			  env, "description", NAPI_AUTO_LENGTH, &key);
			napi_value value;
			napi_create_string_utf8(
			  env, p_kw->desc.c_str(), NAPI_AUTO_LENGTH, &value);

			napi_set_property(env, single_result_dict, key, value);
		}

		// exampleFrames
		{
			napi_value key;
			napi_create_string_utf8(
			  env, "exampleFrames", NAPI_AUTO_LENGTH, &key);
			napi_value value;
			napi_create_array(env, &value);

			size_t ii{ 0 };
			for (auto &&filename : p_kw->top_ex_imgs) {
				// \todo This must not be just IDs, but the
				// whole filepaths.
				/*napi_value val;
				napi_create_string_utf8(env, (path_prefix +
				filename_.data(), NAPI_AUTO_LENGTH, &filename);

				napi_set_element(env, value, ii, val);
				++ii;*/
			}

			napi_set_property(env, single_result_dict, key, value);
		}

		napi_set_element(env, result_arr, i, single_result_dict);

		++i;
	}

	return Napi::Object(env, result_arr);
}

Napi::Value
SomHunterNapi::is_som_ready(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();
	if (length != 1) {
		Napi::TypeError::New(env,
		                     "Wrong number of parameters "
		                     "(SomHunterNapi::is_som_ready)")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	bool is_ready{ false };
	try {
		debug("API: CALL \n\t som_ready()");

		is_ready = somhunter->get(usr)->som_ready();

		debug(
		  "API: RETURN \n\t som_ready()\n\t\tis_ready = " << is_ready);

	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	napi_value result;
	napi_get_boolean(env, is_ready, &result);

	return Napi::Object(env, result);
}

Napi::Value
SomHunterNapi::get_last_text_query(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();
	if (length != 1) {
		Napi::TypeError::New(env,
		                     "Wrong number of parameters "
		                     "(SomHunterNapi::get_last_text_query)")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();

	napi_value result;
	try {

		auto lquery = somhunter->get(usr)->get_last_text_query();
		napi_create_string_utf8(
		  env, lquery.c_str(), NAPI_AUTO_LENGTH, &result);

	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	return Napi::Object(env, result);
}

Napi::Value
SomHunterNapi::get_level_info(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();

	if (length != 1) {
		Napi::TypeError::New(env, "Wrong number of parameters")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();

	napi_value single_result_dict;
	napi_create_object(env, &single_result_dict);
	try {
		debug("API: CALL \n\t get_level_info\n\t\tusr = " << usr);

		SubmitResult res =
		  somhunter->get(usr)->get_level_info();

		// Corectness
		{

			// Frame
			{
				napi_value fkey;
				napi_create_string_utf8(
				  env, "correctFrame", NAPI_AUTO_LENGTH, &fkey);
				napi_value frame;
				napi_get_boolean(env, res.cFrame, &frame);

				napi_set_property(
				  env, single_result_dict, fkey, frame);
			}
			// Shot
			{
				napi_value skey;
				napi_create_string_utf8(
				  env, "correctShot", NAPI_AUTO_LENGTH, &skey);
				napi_value shot;
				napi_get_boolean(env, res.cShot, &shot);

				napi_set_property(
				  env, single_result_dict, skey, shot);
			}

			// Video
			{
				napi_value vkey;
				napi_create_string_utf8(
				env, "correctVideo", NAPI_AUTO_LENGTH, &vkey);
				napi_value video;
				napi_get_boolean(env, res.cVideo, &video);

				napi_set_property(env, single_result_dict, vkey, video);
			}
		}

		// Points
		{
			napi_value point_arr;
			napi_create_array(env, &point_arr);

			auto point_iter = res.points;
			for (size_t i = 0; i <= res.target_index; ++i, point_iter++) {
				napi_value point;
				napi_create_int64(env, *point_iter, &point);
				napi_set_element(env, point_arr, i, point);
			}

			napi_value key;
			napi_create_string_utf8(
			env, "points", NAPI_AUTO_LENGTH, &key);

			napi_set_property(env, single_result_dict, key, point_arr);
		}

		// Video found on
		{
			napi_value arr;
			napi_create_array(env, &arr);

			auto iter = res.v_found_on;
			for (size_t i = 0; i <= res.target_index; ++i, iter++) {
				napi_value point;
				napi_create_int64(env, *iter, &point);
				napi_set_element(env, arr, i, point);
			}

			napi_value key;
			napi_create_string_utf8(
			env, "videoOn", NAPI_AUTO_LENGTH, &key);

			napi_set_property(env, single_result_dict, key, arr);
		}

		// Shot found on
		{
			napi_value arr;
			napi_create_array(env, &arr);

			auto iter = res.s_found_on;
			for (size_t i = 0; i <= res.target_index; ++i, iter++) {
				napi_value point;
				napi_create_int64(env, *iter, &point);
				napi_set_element(env, arr, i, point);
			}

			napi_value key;
			napi_create_string_utf8(
			env, "shotOn", NAPI_AUTO_LENGTH, &key);

			napi_set_property(env, single_result_dict, key, arr);
		}
		
		// Shot found on
		{
			napi_value arr;
			napi_create_array(env, &arr);

			auto iter = res.f_found_on;
			for (size_t i = 0; i <= res.target_index; ++i, iter++) {
				napi_value point;
				napi_create_int64(env, *iter, &point);
				napi_set_element(env, arr, i, point);
			}

			napi_value key;
			napi_create_string_utf8(
			env, "frameOn", NAPI_AUTO_LENGTH, &key);

			napi_set_property(env, single_result_dict, key, arr);
		}

	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	return Napi::Object(env, single_result_dict);
}

Napi::Value
SomHunterNapi::submit_to_server(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();

	if (length != 2) {
		Napi::TypeError::New(env, "Wrong number of parameters")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();
	ImageId frame_ID{ info[1].As<Napi::Number>().Uint32Value() };

	napi_value single_result_dict;
	napi_create_object(env, &single_result_dict);
	try {
		debug("API: CALL \n\t submit_to_server\n\t\frame_ID = "
		      << frame_ID);

		SubmitResult res =
		  somhunter->get(usr)->submit_to_server(frame_ID);

		// Corectness
		{

			// Frame
			{
				napi_value fkey;
				napi_create_string_utf8(
				  env, "correctFrame", NAPI_AUTO_LENGTH, &fkey);
				napi_value frame;
				napi_get_boolean(env, res.cFrame, &frame);

				napi_set_property(
				  env, single_result_dict, fkey, frame);
			}
			// Shot
			{
				napi_value skey;
				napi_create_string_utf8(
				  env, "correctShot", NAPI_AUTO_LENGTH, &skey);
				napi_value shot;
				napi_get_boolean(env, res.cShot, &shot);

				napi_set_property(
				  env, single_result_dict, skey, shot);
			}

			// Video
			{
				napi_value vkey;
				napi_create_string_utf8(
				env, "correctVideo", NAPI_AUTO_LENGTH, &vkey);
				napi_value video;
				napi_get_boolean(env, res.cVideo, &video);

				napi_set_property(env, single_result_dict, vkey, video);
			}
		}

		// Points
		{
			napi_value point_arr;
			napi_create_array(env, &point_arr);

			auto point_iter = res.points;
			for (size_t i = 0; i <= res.target_index; ++i, point_iter++) {
				napi_value point;
				napi_create_int64(env, *point_iter, &point);
				napi_set_element(env, point_arr, i, point);
			}

			napi_value key;
			napi_create_string_utf8(
			env, "points", NAPI_AUTO_LENGTH, &key);

			napi_set_property(env, single_result_dict, key, point_arr);
		}

		// Video found on
		{
			napi_value arr;
			napi_create_array(env, &arr);

			auto iter = res.v_found_on;
			for (size_t i = 0; i <= res.target_index; ++i, iter++) {
				napi_value point;
				napi_create_int64(env, *iter, &point);
				napi_set_element(env, arr, i, point);
			}

			napi_value key;
			napi_create_string_utf8(
			env, "videoOn", NAPI_AUTO_LENGTH, &key);

			napi_set_property(env, single_result_dict, key, arr);
		}

		// Shot found on
		{
			napi_value arr;
			napi_create_array(env, &arr);

			auto iter = res.s_found_on;
			for (size_t i = 0; i <= res.target_index; ++i, iter++) {
				napi_value point;
				napi_create_int64(env, *iter, &point);
				napi_set_element(env, arr, i, point);
			}

			napi_value key;
			napi_create_string_utf8(
			env, "shotOn", NAPI_AUTO_LENGTH, &key);

			napi_set_property(env, single_result_dict, key, arr);
		}
		
		// Shot found on
		{
			napi_value arr;
			napi_create_array(env, &arr);

			auto iter = res.f_found_on;
			for (size_t i = 0; i <= res.target_index; ++i, iter++) {
				napi_value point;
				napi_create_int64(env, *iter, &point);
				napi_set_element(env, arr, i, point);
			}

			napi_value key;
			napi_create_string_utf8(
			env, "frameOn", NAPI_AUTO_LENGTH, &key);

			napi_set_property(env, single_result_dict, key, arr);
		}

	} catch (const std::exception &e) {
		Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
	}

	return Napi::Object(env, single_result_dict);
}

Napi::Value
SomHunterNapi::report_issue(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	// Process arguments
	int length = info.Length();

	if (length != 1) {
		Napi::TypeError::New(env, "Wrong number of parameters")
		  .ThrowAsJavaScriptException();
	}

	const std::string usr = info[0].As<Napi::String>().Utf8Value();

	somhunter->get(usr)->report_issue();

	return Napi::Object{};
}
