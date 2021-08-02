
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

#ifndef somhunter_h
#define somhunter_h

#include <list>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <filesystem>

#include "AsyncSom.h"
#include "DatasetFeatures.h"
#include "DatasetFrames.h"
#include "KeywordRanker.h"
#include "LikesLogger.h"
#include "RelevanceScores.h"
#include "Submitter.h"

#define PREPARE_IMAGE_ID 0
#define NOT_FOUND_YET 9999
#define SAVE_DIR "user_progress/"

class SomHunter
{
	// *** LOADED DATASET ***
	const Config config;
	const DatasetFeatures *features;
	const KeywordRanker *keywords;

	// *** SEARCH CONTEXT ***
	// Frames state
	DatasetFrames frames;
	// Relevance scores
	ScoreModel scores;

	// Used keyword query
	std::string last_text_query;
	const std::string user;
	const size_t user_order;

	// Relevance feedback context
	std::set<ImageId> likes;
	std::vector<ImageId> shown_images;

	// Current display context
	std::vector<VideoFramePointer> current_display;
	DisplayType current_display_type{ DisplayType::DNull };

	// Previous logged display
	std::vector<VideoFrame> logged_display_frames;

	// asynchronous SOM worker
	AsyncSom asyncSom;

	// VBS logging
	// Submitter submitter;
	UsedTools used_tools;

	// Relevance feedback logging
	ImageId targetId;
	VideoFrame targetFrame;
	FeedbackLogger flogger;

	//std::mt19937 gen;
	//std::uniform_int_distribution<int> distrib;

	std::vector<ImageId> targets;
	std::vector<size_t> points;
	std::vector<size_t> v_found_on;
	std::vector<size_t> s_found_on;
	std::vector<size_t> f_found_on;
	size_t target_index;

	std::mutex mutex;

public:
	SomHunter() = delete;
	/** The main ctor with the filepath to the JSON config file */
	inline SomHunter(const std::string usr,
	                 const Config &cfg,
	                 const DatasetFeatures *feats,
	                 const KeywordRanker *kws,
					 const size_t user_order)
	  : config(cfg)
	  , features(feats)
	  , keywords(kws)
	  , frames(cfg)
	  , scores(frames)
	  , asyncSom(cfg)
	  //, submitter(cfg.submitter_config)
	  , flogger(usr, cfg)
	  //, gen(666)
	  //, distrib(0, features->size())
	  , target_index(0)
	  , targetId(0)
	  , targetFrame(frames.get_frame(0))
	  , user(usr)
	  , user_order(user_order)
	{
		std::ifstream in(config.target_list_file);
		if (!in.good()) {
			warn("Failed to open " << config.target_list_file);
			throw std::runtime_error("missing image list");
		}
		{ 
			#if 0
			targets.push_back(PREPARE_IMAGE_ID);
			#endif
			for (std::string s; getline(in, s);) {
				targets.push_back(std::stoi(s));
			}
			targets.push_back(PREPARE_IMAGE_ID);
		}

		points.resize(targets.size());
		v_found_on.resize(targets.size(), NOT_FOUND_YET);
		s_found_on.resize(targets.size(), NOT_FOUND_YET);
		f_found_on.resize(targets.size(), NOT_FOUND_YET);

		load_state();

		targetId = targets[target_index];
		targetFrame = frames.get_frame(targetId);
		asyncSom.start_work(*features, scores);
		debug("New target id = " << targetId);

	}

	inline const std::string & get_last_text_query() { return last_text_query; }

	/** Returns display of desired type
	 *
	 *	Some diplays may even support paging (e.g. top_n) or
	 * selection of one frame (e.g. top_knn)
	 */
	FramePointerRange get_display(DisplayType d_type,
	                              ImageId selected_image = 0,
	                              PageId page = 0);

	PreviousDisplay get_previous_display();

	void add_likes(const std::vector<ImageId> &likes);

	void remove_likes(const std::vector<ImageId> &likes);

	std::vector<const Keyword *> autocomplete_keywords(
	  const std::string &prefix,
	  size_t count) const;

	/**
	 * Applies all algorithms for score
	 * computation and updates context.
	 */
	DisplayType rescore(const std::string &text_query);

	bool som_ready() const;

	/** Sumbits frame with given id to VBS server */
	SubmitResult submit_to_server(ImageId frame_id);

	/** Returns information about success rate */
	SubmitResult get_level_info();

	/** Resets current search context and starts new search */
	DisplayType reset_search_session();

	/** Returns the only available display type */
	DisplayType get_available_display();

	/** Returns pointer to target frame */
	VideoFramePointer get_target_image();

	/** This display sucks... */
	void report_issue();

private:
	/**
	 *	Applies text query from the user.
	 */
	void rescore_keywords(const std::string &query);

	/**
	 *	Applies feedback from the user based
	 * on shown_images.
	 */
	void rescore_feedback();

	/**
	 *	Gives SOM worker new work.
	 */
	void som_start();

	void save_state();

	void load_state();

	FramePointerRange get_random_display();

	FramePointerRange get_topn_display(PageId page);

	FramePointerRange get_topn_context_display(PageId page);

	FramePointerRange get_som_display();

	FramePointerRange get_video_detail_display(ImageId selected_image);

	FramePointerRange get_topKNN_display(ImageId selected_image,
	                                     PageId page);

	// Gets only part of last display
	FramePointerRange get_page_from_last(PageId page);

	void reset_scores();
};

/* This is the main backend class. */

class SomHuntersGuild
{

	// Hunter id for each key
	std::unordered_map<std::string, std::unique_ptr<SomHunter>>
	  id_to_hunter;
	std::mutex h_mutex;

	// *** DATASET ***
	const Config cfg;
	const DatasetFeatures features;
	const KeywordRanker kws;

	size_t hunter_count;

public:
	SomHuntersGuild() = delete;

	inline SomHuntersGuild(const Config &cfg)
	  : cfg(cfg)
	  , features(DatasetFrames(cfg),
	             cfg) // TODO is it really necessary to load all frames?
	  , kws(cfg)
	  , hunter_count(0)
	{

		if (!std::filesystem::is_directory(SAVE_DIR))
			std::filesystem::create_directory(SAVE_DIR);

		for (const auto & entry : std::filesystem::directory_iterator(SAVE_DIR)) {
			debug("Scanning " << entry);
			if (entry.is_regular_file()) {
				// Load name
				std::string p = entry.path().stem().string();

				// Parse name and position
				auto split = p.find("-");
				std::string id = p.substr(0, split);
				size_t pos = std::stol(p.substr(split + 1));

				// Create hunter
				id_to_hunter.emplace(
				std::make_pair(std::string(id),
								std::make_unique<SomHunter>(
								id, cfg, &features, &kws, pos)));
				++hunter_count;
			}
		}
		debug("SomHuntersGuild created");
	}

	inline SomHunter *get(const std::string &id)
	{
		if (id_to_hunter.find(id) == id_to_hunter.end()) {
			const std::lock_guard<std::mutex> lock(h_mutex);
			id_to_hunter.emplace(
			  std::make_pair(std::string(id),
			                 std::make_unique<SomHunter>(
			                   id, cfg, &features, &kws, ++hunter_count)));
			info("New hunter was created with id " << id);
		}
		return id_to_hunter[id].get();
	}
};

#endif
