
#include <filesystem>
#include <fstream>
#include <math.h>

#include "DatasetFeatures.h"
#include "LikesLogger.h"
#include "RelevanceScores.h"


#define clamp(x, min, max) (x < min ? min : (x > max ? max : x))

const std::string FeedbackLogger::FEEDBACK = "feedback";
const std::string FeedbackLogger::RESET = "reset";
const std::string FeedbackLogger::TEXT = "text";
const std::string FeedbackLogger::GUESS = "guess";
const std::string FeedbackLogger::ISSUE = "issue";

const std::string FeedbackLogger::DISPLAY_SOM = "som";
const std::string FeedbackLogger::DISPLAY_TOP = "top";

void
FeedbackLogger::log_feedback(const std::string &type,
                             const std::string &display_type,
                             const std::string &query,
                             const ImageId target,
                             const std::vector<ImageId> &display,
                             const std::set<ImageId> &likes,
                             const DatasetFeatures &features,
                             const ScoreModel &scores,
                             const DatasetFrames &frames,
                             const ImageId guess)
{
	if (!std::filesystem::is_directory(usr_log_dir))
		warn("wtf, directory was not created");

	{
		info("Logging " << type << ", target " << target);

		auto curr_time = timestamp();

		std::string path = usr_log_dir + std::string("/") +
		                   std::to_string(curr_time) + ".csv";
		std::ofstream o(path.c_str(), std::ios::app);
		if (!o) {
			warn("Could not write a log file!");
		} else {
			// set formatting
			o << std::fixed << std::setprecision(8);

			// TYPE and display type
			o << type << "," << display_type << ",";

			// USER, iterations, timestamp, and timediff
			o << user << "," << curr_iter << "," << target_iter
			  << "," << curr_time << ","
			  << (curr_time - laction_time) / 1000.0 << ",";

			// update last action time
			laction_time = curr_time;

			// TARGET and its stuff
			auto target_frame = frames.get_frame(target);

			// find best scored image from video
			auto target_video =
			  frames.get_all_video_frames(target_frame.video_ID);
			float video_best_score = scores[target];
			ImageId video_best_image = target;
			for (auto iter = target_video.begin();
			     iter != target_video.end();
			     iter++) {
				if (video_best_score < scores[iter->frame_ID]) {
					video_best_score =
					  scores[iter->frame_ID];
					video_best_image = iter->frame_ID;
				}
			}

			o << target << "," << target_frame.shot_ID << ","
			  << target_frame.video_ID << "," << scores[target]
			  << "," << scores.rank_of_image(target) << ","
			  << scores.rank_of_image(video_best_image) << ",";

			o << query << ",";

			// DISPLAY
			for (auto &&img : display) {
				auto img_frame = frames.get_frame(img);
				float dist = features.d_dot(target, img);
				o << img << "," << img_frame.shot_ID << ","
				  << img_frame.video_ID << "," << std::boolalpha
				  << (likes.find(img) != likes.end()) << ","
				  << dist << "," << scores[img] << ",";
			}

			// histogram computing
			int histogram[HISTOGRAM_BINS];
			float bin_part = 2.0f / HISTOGRAM_BINS;

			for (size_t i = 0; i < HISTOGRAM_BINS; ++i)
				histogram[i] = 0;
			for (auto &&img1 : display) {
				for (auto &&img2 : display) {
					if (img1 > img2) {
						float dist =
						  features.d_dot(img1, img2);
						histogram[clamp(
						  (int)floor(dist / bin_part),
						  0,
						  HISTOGRAM_BINS - 1)]++;
					}
				}
			}

			for (size_t i = 0; i < HISTOGRAM_BINS; ++i)
				o << histogram[i] << ",";

			// GUESS
			if (guess != (IMAGE_ID_ERR_VAL)) {
				auto guess_frame = frames.get_frame(guess);
				o << guess << "," << guess_frame.shot_ID << ","
				  << guess_frame.video_ID << ",";
			} else
				o << "-1,-1,-1,";

			o << std::endl;
		}
	}
	// Update iters
	if (type == FeedbackLogger::RESET)
		target_iter = 0;

	if (type == FeedbackLogger::RESET || type == FeedbackLogger::TEXT)
		curr_iter = 0;

	if (type == FeedbackLogger::TEXT || type == FeedbackLogger::FEEDBACK) {
		target_iter++;
		curr_iter++;
	}
}
