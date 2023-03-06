#include <deus.hpp>
#include <cstdlib>

namespace epos {

class application {
public:
  application(std::string_view cmd)
  {
    if (const auto rv = device_.create(); !rv) {
      throw std::system_error(rv.error(), "create");
    }
  }

  application(application&& other) = delete;
  application(const application& other) = delete;
  application& operator=(application&& other) = delete;
  application& operator=(const application& other) = delete;

  ~application() = default;

  void test()
  {
    const std::vector<unsigned char> data{
      0x00, 0xDE, 0x01, 0xBE, 0x0EF, 0x00, 0x00, 0x42, 0x00,
    };

    if (const auto rv = device_.open(GetCurrentProcessId()); !rv) {
      throw std::system_error(rv.error(), "open");
    }

    const auto regions = device_.query();
    if (!regions) {
      throw std::system_error(regions.error(), "query");
    }

    std::string report;
    const deus::signature signature("DE ?? BE EF 00");
    for (auto& region : *regions) {
      const auto begin = region.base_address;
      const auto end = region.base_address + region.region_size;
      const auto scan = device_.scan(begin, end, signature, [&](UINT_PTR address) {
        unsigned char value = 0;
        const auto read = device_.read(address + 6, &value, sizeof(value));
        if (!read) {
          throw std::system_error(read.error(), "read");
        }
        report.append(std::format("{:02X} ", value));
        return true;
      });
      if (!scan) {
        throw std::system_error(scan.error(), "scan");
      }
    }
    MessageBox(nullptr, report.data(), "EPOS Scan", MB_OK);
  }

private:
  deus::device device_;
};

}  // namespace epos

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPSTR cmd, _In_ int show)
{
  try {
    epos::application application(cmd);
    application.test();
  }
  catch (const std::system_error& e) {
    const auto name = e.code().category().name();
    const auto text = std::format("Could not start application.\r\n\r\n{} {}", name, e.what());
    MessageBox(nullptr, text.data(), "EPOS Error", MB_OK);
    return EXIT_FAILURE;
  }
  catch (const std::exception& e) {
    const auto text = std::format("Could not start application.\r\n\r\n{}", e.what());
    MessageBox(nullptr, text.data(), "EPOS Error", MB_OK);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}