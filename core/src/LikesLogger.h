#ifndef likeslogger_h
#define likeslogger_h

#include <vector>
#include <string>
#include <set>

#include "config.h"
#include "log.h"
#include "config_json.h"

class FeedbackLogger
{
    std::string log_dir;

public:

    inline FeedbackLogger(const Config &cfg) : log_dir(cfg.feedback_log_dir) 
    {
        debug("FeedbackLogger created");
    }

    void log_feedback(ImageId target, const std::set<ImageId> & display, const std::set<ImageId> & likes) const;
};


#endif