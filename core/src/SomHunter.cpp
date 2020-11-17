
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

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include "SomHunter.h"

#include "log.h"
#include "utils.h"

FramePointerRange
SomHunter::get_display(DisplayType d_type, ImageId selected_image, PageId page)
{
	// submitter.poll();

	switch (d_type) {
		case DisplayType::DRand:
			return get_random_display();

		case DisplayType::DTopN:
			return get_topn_display(page);

		case DisplayType::DTopNContext:
			return get_topn_context_display(page);

		case DisplayType::DSom:
			return get_som_display();

		case DisplayType::DVideoDetail:
			return get_video_detail_display(selected_image);

		case DisplayType::DTopKNN:
			return get_topKNN_display(selected_image, page);

		default:
			warn("Unsupported display requested.");
#ifndef NDEBUG
			throw std::runtime_error(
			  "Unsupported display requested.");
#endif // !NDEBUG

			break;
	}

	return FramePointerRange();
}

void
SomHunter::add_likes(const std::vector<ImageId> &likes)
{
	// submitter.poll();

	for (auto ii : likes) {
		this->likes.insert(ii);

		frames.get_frame(ii).liked = true;

		// submitter.log_like(frames, current_display_type, ii);
	}
}

void
SomHunter::remove_likes(const std::vector<ImageId> &likes)
{
	// submitter.poll();

	for (auto ii : likes) {
		this->likes.erase(ii);

		frames.get_frame(ii).liked = false;

		// submitter.log_dislike(frames, current_display_type, ii);
	}
}

std::vector<const Keyword *>
SomHunter::autocomplete_keywords(const std::string &prefix, size_t count) const
{
	// Get the keywrods IDs
	auto kw_IDs{ keywords->find(prefix, count) };

	// Create vector of ptrs to corresponding keyword instances
	std::vector<const Keyword *> res;
	res.reserve(kw_IDs.size());
	for (auto &&kw_ID : kw_IDs) {
		res.emplace_back(&((*keywords)[kw_ID.first]));
	}

	return res;
}

void
SomHunter::rescore(const std::string &text_query)
{
	std::lock_guard<std::mutex> guard(mutex);
	std::string former_text_query = last_text_query;

	// For simplicity, every change of text query
	// resets scores and ignore likes
	if (former_text_query != text_query)
		likes.clear();
	// submitter.poll();

	// Rescore text query
	rescore_keywords(text_query);

	// Rescore relevance feedback
	rescore_feedback();

	// Start SOM computation
	som_start();

	std::string last_displ = FeedbackLogger::DISPLAY_TOP;
	if (current_display_type == DisplayType::DSom)
		last_displ = FeedbackLogger::DISPLAY_SOM;
	// Log relevance feedback and text query
	if (former_text_query != text_query)
		flogger.log_feedback(FeedbackLogger::TEXT,
		                     last_displ,
		                     text_query,
		                     targetId,
		                     shown_images,
		                     likes,
		                     *features,
		                     scores,
		                     frames,
		                     IMAGE_ID_ERR_VAL);

	if (!likes.empty())
		flogger.log_feedback(FeedbackLogger::FEEDBACK,
		                     last_displ,
		                     text_query,
		                     targetId,
		                     shown_images,
		                     likes,
		                     *features,
		                     scores,
		                     frames,
		                     IMAGE_ID_ERR_VAL);

	// Update search context
	shown_images.clear();

	// Reset likes
	likes.clear();
	for (auto &fr : frames) {
		fr.liked = false;
	}

	/*auto top_n = scores.top_n(frames,
	                          TOPN_LIMIT,
	                          config.topn_frames_per_video,
	                          config.topn_frames_per_shot);*/
	/*submitter.submit_and_log_rescore(frames,
	                                 scores,
	                                 used_tools,
	                                 current_display_type,
	                                 top_n,
	                                 last_text_query,
	                                 config.topn_frames_per_video,
	                                 config.topn_frames_per_shot);*/
}

bool
SomHunter::som_ready() const
{
	return asyncSom.map_ready();
}

std::tuple<bool, bool, bool>
SomHunter::submit_to_server(ImageId frame_id)
{
	std::lock_guard<std::mutex> guard(mutex);
	std::string last_displ = FeedbackLogger::DISPLAY_TOP;
	if (current_display_type == DisplayType::DSom)
		last_displ = FeedbackLogger::DISPLAY_SOM;
	// submitter.submit_and_log_submit(frames, current_display_type,
	// frame_id);
	flogger.log_feedback(FeedbackLogger::GUESS,
	                     last_displ,
	                     last_text_query,
	                     targetId,
	                     shown_images,
	                     likes,
	                     *features,
	                     scores,
	                     frames,
	                     frame_id);
	auto guess = frames.get_frame(frame_id);
	return { guess.frame_ID == targetFrame.frame_ID,
		 guess.video_ID == targetFrame.video_ID &&
		   guess.shot_ID == targetFrame.shot_ID,
		 guess.video_ID == targetFrame.video_ID };
}

DisplayType
SomHunter::get_available_display()
{
	if ((target_index % 2) == 1) {
		debug("New avail display is TopN");
		return DisplayType::DTopN;
	} else {
		debug("New avail display is SOM");
		return DisplayType::DSom;
	}
}

DisplayType
SomHunter::reset_search_session()
{
	std::lock_guard<std::mutex> guard(mutex);
	// submitter.poll();

	std::string last_displ = FeedbackLogger::DISPLAY_TOP;
	if (current_display_type == DisplayType::DSom)
		last_displ = FeedbackLogger::DISPLAY_SOM;

	flogger.log_feedback(FeedbackLogger::RESET,
	                     last_displ,
	                     last_text_query,
	                     targetId,
	                     shown_images,
	                     likes,
	                     *features,
	                     scores,
	                     frames,
	                     IMAGE_ID_ERR_VAL);

	reset_scores();

	shown_images.clear();

	// submitter.log_reset_search();
	som_start();

	++target_index;
	if (target_index >= targets.size())
		--target_index;
	targetId = targets[target_index];
	targetFrame = frames.get_frame(targetId);
	debug("New target id = " << targetId
	                         << ", target index = " << target_index);

	return get_available_display();
}

void
SomHunter::rescore_keywords(const std::string &query)
{
	// Do not rescore if query did not change
	if (last_text_query == query) {
		return;
	}

	reset_scores();

	keywords->rank_sentence_query(query, scores, *features, frames, config);

	last_text_query = query;
	used_tools.KWs_used = true;

	// submitter.log_add_keywords(query);
}

void
SomHunter::rescore_feedback()
{
	if (likes.empty())
		return;

	scores.apply_bayes(likes, shown_images, *features);
	used_tools.bayes_used = true;
}

void
SomHunter::som_start()
{
	asyncSom.start_work(*features, scores);
}

VideoFramePointer
SomHunter::get_target_image()
{
	return &targetFrame;
}

FramePointerRange
SomHunter::get_random_display()
{
	// Get ids
	auto ids = scores.weighted_sample(
	  DISPLAY_GRID_WIDTH * DISPLAY_GRID_HEIGHT, RANDOM_DISPLAY_WEIGHT);

	// Log
	// submitter.log_show_random_display(frames, ids);
	// Update context
	shown_images.clear();
	for (auto id : ids)
		shown_images.push_back(id);
	current_display = frames.ids_to_video_frame(ids);
	current_display_type = DisplayType::DRand;

	return FramePointerRange(current_display);
}

std::vector<std::string>
split(std::string s, std::string delimiter)
{
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	std::string token;
	std::vector<std::string> res;

	while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
		token = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		res.push_back(token);
	}

	res.push_back(s.substr(pos_start));
	return res;
}

PreviousDisplay
SomHunter::get_previous_display()
{

	//	Get log path
	auto path_to_logs =
	  std::filesystem::current_path() / ("feedback_log") / user;

	//	list all files in log directory and sort them(from oldest to
	// newest)
	std::vector<std::string> log_file_names;
	std::string ext(".csv");
	for (auto &p :
	     std::filesystem::recursive_directory_iterator(path_to_logs)) {
		if (p.path().extension() == ext)
			log_file_names.emplace_back(p.path().stem().string());
	}
	std::sort(log_file_names.begin(), log_file_names.end());

	//	try to load the newest log file and read data
	std::string csv_data;
	int index = log_file_names.size() - 1;

	// Find newest file
	while (index > 0) {

		std::string newest_file_name = log_file_names[index];

		// Read file
		auto filepath = path_to_logs / (newest_file_name + ".csv");
		std::ifstream newest_file(filepath);

		if (newest_file.is_open()) {
			getline(newest_file, csv_data);
			newest_file.close();
			break;
		}

		index--;
	}

	// If there is no suitable, then return non-valid range
	if (index <= -1) {
		return PreviousDisplay();
	}

	//	extract IDs from csv data
	std::vector<ImageId> ids;
	auto splitted_csv_data = split(csv_data, ",");
	for (size_t i = 0; i < DISPLAY_GRID_WIDTH * DISPLAY_GRID_HEIGHT; i++) {
		int index = 14 + (i * 6);
		ids.emplace_back((ImageId)std::stol(splitted_csv_data[index]));
	}

	// 15-th column (14th index)
	// + 3 if selected
	// + 6 positions is next id

	// Create copy of last logged display frames
	logged_display_frames = frames.ids_copy_video_frame(ids);

	// Create FramePointerRange
	logged_display.clear();
	logged_display.reserve(ids.size());
	for (size_t i = 0; i < DISPLAY_GRID_WIDTH * DISPLAY_GRID_HEIGHT; i++) {
		if (ids[i] == IMAGE_ID_ERR_VAL) {
			logged_display.push_back(nullptr);
			continue;
		}

		logged_display.push_back(&logged_display_frames[i]);
	}

	// Apply former likes
	for (size_t i = 0; i < DISPLAY_GRID_WIDTH * DISPLAY_GRID_HEIGHT; i++) {
		int index = 14 + (i * 6) + 3;
		logged_display_frames[i].liked =
		  splitted_csv_data[index] == "true";
	}
	
	previousDisplay.display = logged_display;

	// get target image
	// 8th column is target image id (7th index)
	size_t target_image_id_index = 7;
	ImageId loggedTargetID = (ImageId)std::stol(splitted_csv_data[target_image_id_index]);
	previousDisplay.target = &frames.get_frame(loggedTargetID);

	// get cosine distance
	// extract IDs from csv data
	for (size_t i = 0; i < DISPLAY_GRID_WIDTH * DISPLAY_GRID_HEIGHT; i++) {
		int index = 14 + (i * 6) + 4;
		distances.emplace_back(std::stof(splitted_csv_data[index]));
	}

	return previousDisplay;
}

FramePointerRange
SomHunter::get_topn_display(PageId page)
{
	// Another display or first page -> load
	if (current_display_type != DisplayType::DTopN || page == 0) {
		// Get ids
		auto ids = scores.top_n(frames,
		                        TOPN_LIMIT,
		                        config.topn_frames_per_video,
		                        config.topn_frames_per_shot);

		// Log
		// submitter.log_show_topn_display(frames, ids);

		// Update context
		current_display = frames.ids_to_video_frame(ids);
		current_display_type = DisplayType::DTopN;
	}

	return get_page_from_last(page);
}

FramePointerRange
SomHunter::get_topn_context_display(PageId page)
{
	// Another display or first page -> load
	if (current_display_type != DisplayType::DTopNContext || page == 0) {
		debug("Loading top n context display first page");
		// Get ids
		auto ids =
		  scores.top_n_with_context(frames,
		                            TOPN_LIMIT,
		                            config.topn_frames_per_video,
		                            config.topn_frames_per_shot);

		// Log
		// submitter.log_show_topn_context_display(frames, ids);

		// Update context
		current_display = frames.ids_to_video_frame(ids);
		current_display_type = DisplayType::DTopNContext;
	}

	return get_page_from_last(page);
}

FramePointerRange
SomHunter::get_som_display()
{
	if (!asyncSom.map_ready()) {
		return FramePointerRange();
	}

	std::vector<ImageId> ids;
	ids.resize(SOM_DISPLAY_GRID_WIDTH * SOM_DISPLAY_GRID_HEIGHT);

	for (size_t i = 0; i < SOM_DISPLAY_GRID_WIDTH; ++i) {
		for (size_t j = 0; j < SOM_DISPLAY_GRID_HEIGHT; ++j) {
			if (asyncSom.map(i + SOM_DISPLAY_GRID_WIDTH * j)
			      .empty()) {
				ids[i + SOM_DISPLAY_GRID_WIDTH * j] =
				  IMAGE_ID_ERR_VAL;
			} else {
				ImageId id = scores.weighted_example(
				  asyncSom.map(i + SOM_DISPLAY_GRID_WIDTH * j));
				ids[i + SOM_DISPLAY_GRID_WIDTH * j] = id;
			}
		}
	}

	std::unordered_map<size_t, size_t> stolen_count;
	for (size_t i = 0; i < SOM_DISPLAY_GRID_WIDTH * SOM_DISPLAY_GRID_HEIGHT;
	     ++i) {
		stolen_count.emplace(i, 1);
	}

	for (size_t i = 0; i < SOM_DISPLAY_GRID_WIDTH; ++i) {
		for (size_t j = 0; j < SOM_DISPLAY_GRID_HEIGHT; ++j) {
			if (asyncSom.map(i + SOM_DISPLAY_GRID_WIDTH * j)
			      .empty()) {
				debug("Fixing cluster "
				      << i + SOM_DISPLAY_GRID_WIDTH * j);
				auto k = asyncSom.get_koho(
				  i + SOM_DISPLAY_GRID_WIDTH * j);

				size_t clust =
				  asyncSom.nearest_cluster_with_atleast(
				    k, stolen_count);

				stolen_count[clust]++;
				std::vector<ImageId> ci = asyncSom.map(clust);

				for (ImageId ii : ids) {
					auto fi =
					  std::find(ci.begin(), ci.end(), ii);
					if (fi != ci.end())
						ci.erase(fi);
				}

				assert(!ci.empty());

				ImageId id = scores.weighted_example(ci);
				ids[i + SOM_DISPLAY_GRID_WIDTH * j] = id;
			}
		}
	}

	// Log
	// submitter.log_show_som_display(frames, ids);

	// Update context
	shown_images.clear();
	for (auto id : ids) {
		if (id == IMAGE_ID_ERR_VAL)
			continue;

		shown_images.push_back(id);
	}
	current_display = frames.ids_to_video_frame(ids);
	current_display_type = DisplayType::DSom;

	return FramePointerRange(current_display);
}

FramePointerRange
SomHunter::get_video_detail_display(ImageId selected_image)
{
	VideoId v_id = frames.get_video_id(selected_image);

	if (v_id == VIDEO_ID_ERR_VAL) {
		warn("Video for " << selected_image << " not found");
		return std::vector<VideoFramePointer>();
	}

	// Get ids
	FrameRange video_frames = frames.get_all_video_frames(v_id);

	// Log
	// submitter.log_show_detail_display(frames, selected_image);

	// Update context
	for (auto iter = video_frames.begin(); iter != video_frames.end();
	     ++iter) {
		shown_images.push_back(iter->frame_ID);
	}

	current_display = frames.range_to_video_frame(video_frames);
	current_display_type = DisplayType::DVideoDetail;

	return FramePointerRange(current_display);
}

FramePointerRange
SomHunter::get_topKNN_display(ImageId selected_image, PageId page)
{
	// Another display or first page -> load
	if (current_display_type != DisplayType::DTopKNN || page == 0) {
		debug("Getting KNN for image " << selected_image);
		// Get ids
		auto ids = features->get_top_knn(frames,
		                                 selected_image,
		                                 config.topn_frames_per_video,
		                                 config.topn_frames_per_shot);

		debug("Got result of size " << ids.size());
		// Log
		// submitter.log_show_topknn_display(frames, selected_image,
		// ids);

		// Update context
		current_display = frames.ids_to_video_frame(ids);
		current_display_type = DisplayType::DTopKNN;

		// KNN is query by example so we NEED to log a rerank
		UsedTools ut;
		ut.topknn_used = true;

		/*submitter.submit_and_log_rescore(frames,
		                                 scores,
		                                 ut,
		                                 current_display_type,
		                                 ids,
		                                 last_text_query,
		                                 config.topn_frames_per_video,
		                                 config.topn_frames_per_shot);*/
	}

	return get_page_from_last(page);
}

FramePointerRange
SomHunter::get_page_from_last(PageId page)
{
	debug("Getting page "
	      << page << ", page size " << config.display_page_size
	      << ", current display size " << current_display.size());

	size_t begin_off{ std::min(current_display.size(),
		                   page * config.display_page_size) };
	size_t end_off{ std::min(current_display.size(),
		                 page * config.display_page_size +
		                   config.display_page_size) };

	FramePointerRange res(current_display.cbegin() + begin_off,
	                      current_display.cbegin() + end_off);

	// Update context
	shown_images.clear();
	for (auto iter = res.begin(); iter != res.end(); ++iter)
		shown_images.push_back((*iter)->frame_ID);

	return res;
}

void
SomHunter::reset_scores()
{
	used_tools.reset();

	// shown_images.clear();

	// Reset likes
	likes.clear();
	for (auto &fr : frames) {
		fr.liked = false;
	}

	last_text_query = "";

	scores.reset();
}

void
SomHunter::report_issue()
{
	std::lock_guard<std::mutex> guard(mutex);
	std::string last_displ = FeedbackLogger::DISPLAY_TOP;
	if (current_display_type == DisplayType::DSom)
		last_displ = FeedbackLogger::DISPLAY_SOM;

	flogger.log_feedback(FeedbackLogger::ISSUE,
	                     last_displ,
	                     last_text_query,
	                     targetId,
	                     shown_images,
	                     likes,
	                     *features,
	                     scores,
	                     frames,
	                     IMAGE_ID_ERR_VAL);
}
