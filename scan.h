/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __SCAN_H
#define __SCAN_H

#include <string>

namespace utils
{
    struct meta_data
    {
        std::string title;
        std::string episode;
        std::string year;
        std::string genre;
        std::string plot;
        std::string imdb_rating;
        std::string imdb_id;
        std::string poster;
    };

    bool get_meta_data_by_filename(const std::string& filename,meta_data& meta);

    bool omdb_fetch_meta_data(meta_data& meta);

    int scan_for_media(void);

    int update_media_meta(void);
}

#endif
