
#include <fstream>
#include <filesystem>

#include "LikesLogger.h"
#include "DatasetFeatures.h"
#include "RelevanceScores.h"

const std::string FeedbackLogger::FEEDBACK = "feedback";
const std::string FeedbackLogger::RESET = "reset";
const std::string FeedbackLogger::TEXT = "text";
const std::string FeedbackLogger::GUESS = "guess";

void
FeedbackLogger::log_feedback(const std::string &type,
                             const std::string &query,
                             const ImageId target,
                             const std::set<ImageId> &display,
                             const std::set<ImageId> &likes,
                             const DatasetFeatures &features,
                             const ScoreModel &scores,
                             const DatasetFrames &frames,
                             const ImageId guess)
{
	if (likes.empty())
		return;

	if (!std::filesystem::is_directory(usr_log_dir))
		warn("wtf, directory was not created");

	{
        auto curr_time = timestamp();
		std::string path = usr_log_dir + std::string("/") +
		                   std::to_string(curr_time) + ".csv";
		std::ofstream o(path.c_str(), std::ios::app);
		if (!o) {
			warn("Could not write a log file!");
		} else {
            // USER, iterations, timestamp, and timediff
            o << user << ","
              << curr_iter << ","
              << target_iter << ","
              << curr_time << ","
              << curr_time - laction_time << ",";
            
            // update last action time
            laction_time = curr_time;

            // TARGET and its stuff
            o << target << ","
              << scores[target] << ","
              << scores.rank_of_image(target) << ",";

            // DISPLAY
            for (auto && img : display) {
                auto img_frame = frames.get_frame(img);
                o << img << ","
                  << img_frame.shot_ID << ","
                  << img_frame.video_ID << ","
                  << std::boolalpha << (likes.find(img) != likes.end()) << ","
                  << features.d_dot(target, img) << ","
                  << scores[img] << ",";
            }

            // GUESS
            if (guess != (IMAGE_ID_ERR_VAL)) {
                auto guess_frame = frames.get_frame(guess);
                o << guess << ","
                << guess_frame.shot_ID << ","
                << guess_frame.video_ID << ",";
            } else 
                o << "-1,-1,-1,";


		}
	}
}
