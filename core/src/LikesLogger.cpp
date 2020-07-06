
#include <filesystem>
#include <fstream>

#include "LikesLogger.h"

void FeedbackLogger::log_feedback(ImageId target, 
    const std::set<ImageId> & display, 
    const std::set<ImageId> & likes) const 
    {
        if (likes.empty())
            return;

        if (!std::filesystem::is_directory(log_dir))
		std::filesystem::create_directory(log_dir);

        if (!std::filesystem::is_directory(log_dir))
            warn("wtf, directory was not created");

        {
            std::string path = log_dir +
                            std::string("/") +
                            std::to_string(timestamp()) +
                            ".json";
            std::ofstream o(path.c_str(), std::ios::app);
            if (!o) {
                warn("Could not write a log file!");
            } else {
                o << "{"
                << "\"target\": " << target << ","
                << std::endl
                << "\"display\": [";
                bool first = true;
                for (auto && ii : display)
                {
                    if (!first)
                        o << ",";
                    first = false;
                    o << ii;
                }
                o << "],"
                << std::endl
                << "\"likes\": [";
                first = true;
                for (auto && ii : likes)
                {
                    if (!first)
                        o << ",";
                    first = false;
                    o << ii;
                }
                o << "]"
                << std::endl;

                o << "}" << std::endl;
            }
        }
    }

