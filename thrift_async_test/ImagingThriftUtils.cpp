#include "CommonHeader.h"

#ifdef WIN32
#define snprintf _snprintf_c
#define PATH_MAX MAX_PATH
#else
#include <unistd.h>
#endif

#include "ImagingThriftUtils.h"

#include <boost/thread/mutex.hpp>

using namespace imaging;

using namespace boost;
using namespace boost::gil;
using namespace imaging;

static boost::mutex server_mutex;

namespace imaging {

#ifdef _WIN32
boost::filesystem::path get_tmp_filename()
{
	char temp_dir[PATH_MAX];
	char filename[PATH_MAX];
    GetTempPathA(PATH_MAX, temp_dir);
    GetTempFileNameA(temp_dir, "qem", 0, filename);

	return boost::filesystem::path(filename);
}
#else
boost::filesystem::path get_tmp_filename()
{
	char filename[PATH_MAX];
    boost::mutex::scoped_lock	lock(server_mutex);

    int fd;
    const char *tmpdir;
    /* XXX: race condition possible */
    tmpdir = getenv("TMPDIR");
    if (!tmpdir)
        tmpdir = "/tmp";
    snprintf(filename, sizeof(filename), "%s/vl.XXXXXX", tmpdir);
    fd = mkstemp(filename);
    close(fd);

	return boost::filesystem::path(filename);
}
#endif

rgb8_image_t image_from_string(const std::string& img) {
	boost::filesystem::path jpegpath = get_tmp_filename();

	std::ofstream jpegOut(jpegpath.file_string().c_str(), std::ios::out|std::ios::trunc|std::ios::binary);
	jpegOut << img;
	jpegOut.close();

	rgb8_image_t image;
	jpeg_read_and_convert_image(jpegpath.file_string().c_str(), image);

	boost::filesystem::remove(jpegpath);
	return image;
}

} // namespace imaging
