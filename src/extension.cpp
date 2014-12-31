#include "rencpp/values.hpp"
#include "rencpp/extension.hpp"

namespace ren {

namespace internal {
    std::mutex extensionTablesMutex;

    RenShimId shimIdToCapture = -1;

    RenShimBouncer shimBouncerToCapture = nullptr;
}

}
