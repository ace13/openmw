#ifndef COMPONENTS_FILES_FIXEDPATH_HPP
#define COMPONENTS_FILES_FIXEDPATH_HPP

#include <string>
#include <experimental/filesystem>

namespace sfs = std::experimental::filesystem;

#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__)
#ifndef ANDROID
    #include <components/files/linuxpath.hpp>
    namespace Files { typedef LinuxPath TargetPathType; }
#else
    #include <components/files/androidpath.hpp>
    namespace Files { typedef AndroidPath TargetPathType; }
#endif
#elif defined(__WIN32) || defined(__WINDOWS__) || defined(_WIN32)
    #include <components/files/windowspath.hpp>
    namespace Files { typedef WindowsPath TargetPathType; }

#elif defined(macintosh) || defined(Macintosh) || defined(__APPLE__) || defined(__MACH__)
    #include <components/files/macospath.hpp>
    namespace Files { typedef MacOsPath TargetPathType; }

#else
    #error "Unknown platform!"
#endif


/**
 * \namespace Files
 */
namespace Files
{

/**
 * \struct Path
 *
 * \tparam P - Path strategy class type (depends on target system)
 *
 */
template
<
    class P = TargetPathType
>
struct FixedPath
{
    typedef P PathType;

    /**
     * \brief Path constructor.
     *
     * \param [in] application_name - Name of the application
     */
    FixedPath(const std::string& application_name)
        : mPath(application_name + "/")
        , mUserConfigPath(mPath.getUserConfigPath())
        , mUserDataPath(mPath.getUserDataPath())
        , mGlobalConfigPath(mPath.getGlobalConfigPath())
        , mLocalPath(mPath.getLocalPath())
        , mGlobalDataPath(mPath.getGlobalDataPath())
        , mCachePath(mPath.getCachePath())
        , mInstallPath(mPath.getInstallPath())
    {
    }

    /**
     * \brief Return path pointing to the user local configuration directory.
     */
    const sfs::path& getUserConfigPath() const
    {
        return mUserConfigPath;
    }

    const sfs::path& getUserDataPath() const
    {
        return mUserDataPath;
    }

    /**
     * \brief Return path pointing to the global (system) configuration directory.
     */
    const sfs::path& getGlobalConfigPath() const
    {
        return mGlobalConfigPath;
    }

    /**
     * \brief Return path pointing to the directory where application was started.
     */
    const sfs::path& getLocalPath() const
    {
        return mLocalPath;
    }


    const sfs::path& getInstallPath() const
    {
        return mInstallPath;
    }

    const sfs::path& getGlobalDataPath() const
    {
        return mGlobalDataPath;
    }

    const sfs::path& getCachePath() const
    {
        return mCachePath;
    }

    private:
        PathType mPath;

        sfs::path mUserConfigPath;       /**< User path  */
        sfs::path mUserDataPath;
        sfs::path mGlobalConfigPath;     /**< Global path */
        sfs::path mLocalPath;      /**< It is the same directory where application was run */

        sfs::path mGlobalDataPath;        /**< Global application data path */

        sfs::path mCachePath;

        sfs::path mInstallPath;

};


} /* namespace Files */

#endif /* COMPONENTS_FILES_FIXEDPATH_HPP */
