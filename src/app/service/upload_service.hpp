#pragma once

#include <memory>
#include <string>
#include <functional>

namespace app {

class NetSession;
class SpeedLimiter;

/// 上传服务。
class UploadService {
public:
    explicit UploadService(std::shared_ptr<NetSession> session);

    /// 设置上传速度限制（KB/s），0 为不限速。
    void set_upload_speed_limit(int kbps);

    /// 计算文件MD5值。
    static std::string compute_file_md5(const std::string& file_path);

    /// 上传文件。
    /// @param file_path 本地文件路径
    /// @param parent_file_id 目标目录ID
    /// @param dup_choice 重复文件处理策略（0=提示/1=覆盖/2=跳过）
    /// @param progress_callback 进度回调
    /// @param is_cancelled 取消检查函数
    /// @return 上传后的文件ID（成功）或 "已取消"
    std::string up_load(const std::string& file_path, int64_t parent_file_id,
                        int dup_choice = 0,
                        std::function<void(int)> progress_callback = nullptr,
                        std::function<bool()> is_cancelled = nullptr);

private:
    std::shared_ptr<NetSession> _session;
};

}  // namespace app
