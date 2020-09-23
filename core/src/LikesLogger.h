#ifndef likeslogger_h
#define likeslogger_h

#include <chrono>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "DatasetFeatures.h"
#include "RelevanceScores.h"
#include "config.h"
#include "config_json.h"
#include "log.h"

class FeedbackLogger
{
	std::string log_dir;
	std::string user;
	std::string usr_log_dir;

	int64_t laction_time;

	size_t curr_iter;
	size_t target_iter;

public:
	static const std::string FEEDBACK;
	static const std::string RESET;
	static const std::string TEXT;
	static const std::string GUESS;

	inline FeedbackLogger(const std::string &usr, const Config &cfg)
	  : log_dir(cfg.feedback_log_dir)
	  , user(usr)
	  , usr_log_dir(log_dir + std::string("/") + user)
	  , laction_time(timestamp())
	  , curr_iter(0)
	  , target_iter(0)
	{
		if (!std::filesystem::is_directory(log_dir))
			std::filesystem::create_directory(log_dir);

		if (!std::filesystem::is_directory(usr_log_dir))
			std::filesystem::create_directory(usr_log_dir);

		debug("FeedbackLogger and its folders created");
	}

	void log_feedback(const std::string &type,
	                  const std::string &query,
	                  const ImageId target,
	                  const std::vector<ImageId> &display,
	                  const std::set<ImageId> &likes,
	                  const DatasetFeatures &features,
	                  const ScoreModel &scores,
	                  const DatasetFrames &frames,
	                  const ImageId guess);
};

#endif