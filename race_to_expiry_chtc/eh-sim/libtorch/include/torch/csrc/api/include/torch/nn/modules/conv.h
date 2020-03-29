#pragma once

#include <torch/expanding_array.h>
#include <torch/nn/cloneable.h>
#include <torch/nn/init.h>
#include <torch/nn/options/conv.h>
#include <torch/nn/pimpl.h>
#include <torch/types.h>

#include <torch/csrc/WindowsTorchApiMacro.h>

#include <cstddef>
#include <vector>

namespace torch {
namespace nn {

/// Base class for all (dimension-specialized) convolution modules.
template <size_t D, typename Derived>
class ConvNdImpl : public torch::nn::Cloneable<Derived> {
 public:
  explicit ConvNdImpl(detail::ConvNdOptions<D> options_) : options(std::move(options_)) {
    reset();
  }

  void reset() override {
    TORCH_CHECK(
      options.in_channels() % options.groups() == 0,
      "in_channels must be divisible by groups");
    TORCH_CHECK(
      options.out_channels() % options.groups() == 0,
      "out_channels must be divisible by groups");

    if (options.transposed()) {
      std::vector<int64_t> weight_sizes = {
        options.in_channels(),
        options.out_channels() / options.groups()};
      weight_sizes.insert(weight_sizes.end(), (*options.kernel_size()).begin(), (*options.kernel_size()).end());
      weight = this->register_parameter(
        "weight",
        torch::empty(weight_sizes));
    } else {
      std::vector<int64_t> weight_sizes = {
        options.out_channels(),
        options.in_channels() / options.groups()};
      weight_sizes.insert(weight_sizes.end(), (*options.kernel_size()).begin(), (*options.kernel_size()).end());
      weight = this->register_parameter(
        "weight",
        torch::empty(weight_sizes));
    }

    if (options.bias()) {
      bias = this->register_parameter("bias", torch::empty({options.out_channels()}));
    } else {
      this->register_parameter("bias", Tensor(), /*requires_grad=*/false);
    }

    reset_parameters();
  }

  void reset_parameters() {
    init::kaiming_uniform_(weight, /*a=*/std::sqrt(5));  // NOLINT(cppcoreguidelines-avoid-magic-numbers)

    if (bias.defined()) {
      int64_t fan_in, fan_out;
      std::tie(fan_in, fan_out) = init::_calculate_fan_in_and_fan_out(weight);
      auto bound = 1 / std::sqrt(fan_in);
      init::uniform_(bias, -bound, bound);
    }
  }

  /// Pretty prints the `Conv{1,2,3}d` module into the given `stream`.
  void pretty_print(std::ostream& stream) const override {
    stream << "torch::nn::Conv" << D << "d"
           << "(" << options.in_channels()
           << ", " << options.out_channels()
           << ", kernel_size=" << options.kernel_size()
           << ", stride=" << options.stride();
    if (*options.padding() != *ExpandingArray<D>(0)) {
      stream << ", padding=" << options.padding();
    }
    if (*options.dilation() != *ExpandingArray<D>(1)) {
      stream << ", dilation=" << options.dilation();
    }
    if (*options.output_padding() != *ExpandingArray<D>(0)) {
      stream << ", output_padding=" << options.output_padding();
    }
    if (options.groups() != 1) {
      stream << ", groups=" << options.groups();
    }
    if (!options.bias()) {
      stream << ", bias=" << std::boolalpha << false;
    }
    if (!c10::get_if<enumtype::kZeros>(&options.padding_mode())) {
      stream << ", padding_mode=" << enumtype::get_enum_name(options.padding_mode());
    }
    stream << ")";
  }

  /// The options with which this `Module` was constructed.
  detail::ConvNdOptions<D> options;

  /// The learned kernel (or "weight").
  Tensor weight;

  /// The learned bias. Only defined if the `bias` option was true.
  Tensor bias;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Conv1d ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/// Applies convolution over a 1-D input.
/// See https://pytorch.org/docs/master/nn.html#torch.nn.Conv1d to learn about
/// the exact behavior of this module.
class TORCH_API Conv1dImpl : public ConvNdImpl<1, Conv1dImpl> {
 public:
  Conv1dImpl(
      int64_t input_channels,
      int64_t output_channels,
      ExpandingArray<1> kernel_size)
      : Conv1dImpl(Conv1dOptions(input_channels, output_channels, kernel_size)) {
  }
  explicit Conv1dImpl(Conv1dOptions options_);
  Tensor forward(const Tensor& input);
};

/// A `ModuleHolder` subclass for `Conv1dImpl`.
/// See the documentation for `Conv1dImpl` class to learn what methods it
/// provides, or the documentation for `ModuleHolder` to learn about PyTorch's
/// module storage semantics.
TORCH_MODULE(Conv1d);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Conv2d ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/// Applies convolution over a 2-D input.
/// See https://pytorch.org/docs/master/nn.html#torch.nn.Conv2d to learn about
/// the exact behavior of this module.
class TORCH_API Conv2dImpl : public ConvNdImpl<2, Conv2dImpl> {
 public:
  Conv2dImpl(
      int64_t input_channels,
      int64_t output_channels,
      ExpandingArray<2> kernel_size)
      : Conv2dImpl(Conv2dOptions(input_channels, output_channels, kernel_size)) {
  }
  explicit Conv2dImpl(Conv2dOptions options_);
  Tensor forward(const Tensor& input);
};

/// A `ModuleHolder` subclass for `Conv2dImpl`.
/// See the documentation for `Conv2dImpl` class to learn what methods it
/// provides, or the documentation for `ModuleHolder` to learn about PyTorch's
/// module storage semantics.
TORCH_MODULE(Conv2d);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Conv3d ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/// Applies convolution over a 3-D input.
/// See https://pytorch.org/docs/master/nn.html#torch.nn.Conv3d to learn about
/// the exact behavior of this module.
class TORCH_API Conv3dImpl : public ConvNdImpl<3, Conv3dImpl> {
 public:
  Conv3dImpl(
      int64_t input_channels,
      int64_t output_channels,
      ExpandingArray<3> kernel_size)
      : Conv3dImpl(Conv3dOptions(input_channels, output_channels, kernel_size)) {
  }
  explicit Conv3dImpl(Conv3dOptions options_);
  Tensor forward(const Tensor& input);
};

/// A `ModuleHolder` subclass for `Conv3dImpl`.
/// See the documentation for `Conv3dImpl` class to learn what methods it
/// provides, or the documentation for `ModuleHolder` to learn about PyTorch's
/// module storage semantics.
TORCH_MODULE(Conv3d);

// ~~~~~~~~~~~~~~~~~~~~~~~~~ ConvTranspose ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/// Base class for all (dimension-specialized) convolution transpose modules.
template <size_t D, typename Derived>
class ConvTransposeNdImpl : public ConvNdImpl<D, Derived> {
 public:
  using torch::nn::ConvNdImpl<D, Derived>::ConvNdImpl;

  /// Pretty prints the `ConvTranspose{1,2,3}d` module into the given `stream`.
  void pretty_print(std::ostream& stream) const override {
    stream << "torch::nn::ConvTranspose" << D << "d"
           << "(" << this->options.in_channels()
           << ", " << this->options.out_channels()
           << ", kernel_size=" << this->options.kernel_size()
           << ", stride=" << this->options.stride();
    if (*this->options.padding() != *ExpandingArray<D>(0)) {
      stream << ", padding=" << this->options.padding();
    }
    if (*this->options.dilation() != *ExpandingArray<D>(1)) {
      stream << ", dilation=" << this->options.dilation();
    }
    if (*this->options.output_padding() != *ExpandingArray<D>(0)) {
      stream << ", output_padding=" << this->options.output_padding();
    }
    if (this->options.groups() != 1) {
      stream << ", groups=" << this->options.groups();
    }
    if (!this->options.bias()) {
      stream << ", bias=" << std::boolalpha << false;
    }
    if (!c10::get_if<enumtype::kZeros>(&this->options.padding_mode())) {
      stream << ", padding_mode=" << enumtype::get_enum_name(this->options.padding_mode());
    }
    stream << ")";
  }

 protected:
  std::vector<int64_t> _output_padding(
      const Tensor& input, const c10::optional<at::IntArrayRef>& output_size,
      const ExpandingArray<D>& stride, const ExpandingArray<D>& padding,
      const ExpandingArray<D>& kernel_size);
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ConvTranspose1d ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/// Applies the ConvTranspose1d function.
/// See https://pytorch.org/docs/master/nn.html#torch.nn.ConvTranspose1d to
/// learn about the exact behavior of this module.
///
/// NOTE: `ConvTranspose1d` currently cannot be used in a `Sequential` module,
/// because `Sequential` module doesn't support modules with forward method that takes
/// optional arguments. Users should create their own wrapper for `ConvTranspose1d`
/// and have its forward method just accept a tensor, if they want to use it in a
/// `Sequential` module.
///
/// Example:
/// ```
/// struct ConvTranspose1dWrapperImpl : public torch::nn::ConvTranspose1dImpl {
///   using torch::nn::ConvTranspose1dImpl::ConvTranspose1dImpl;
///
///   torch::Tensor forward(const torch::Tensor& input) {
///     return torch::nn::ConvTranspose1dImpl::forward(input, c10::nullopt);
///   }
/// };
///
/// TORCH_MODULE(ConvTranspose1dWrapper);
///
/// torch::nn::Sequential sequential(
///   ConvTranspose1dWrapper(torch::nn::ConvTranspose1dOptions(3, 3, 4));
/// ```
class TORCH_API ConvTranspose1dImpl : public ConvTransposeNdImpl<1, ConvTranspose1dImpl> {
 public:
  ConvTranspose1dImpl(
      int64_t input_channels,
      int64_t output_channels,
      ExpandingArray<1> kernel_size)
      : ConvTranspose1dImpl(ConvTranspose1dOptions(input_channels, output_channels, kernel_size)) {
  }
  explicit ConvTranspose1dImpl(ConvTranspose1dOptions options_);
  Tensor forward(const Tensor& input,
                 const c10::optional<at::IntArrayRef>& output_size = c10::nullopt);
};

TORCH_MODULE(ConvTranspose1d);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ConvTranspose2d ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/// Applies the ConvTranspose2d function.
/// See https://pytorch.org/docs/master/nn.html#torch.nn.ConvTranspose2d to
/// learn about the exact behavior of this module.
///
/// NOTE: `ConvTranspose2d` currently cannot be used in a `Sequential` module,
/// because `Sequential` module doesn't support modules with forward method that takes
/// optional arguments. Users should create their own wrapper for `ConvTranspose2d`
/// and have its forward method just accept a tensor, if they want to use it in a
/// `Sequential` module.
///
/// Example:
/// ```
/// struct ConvTranspose2dWrapperImpl : public torch::nn::ConvTranspose2dImpl {
///   using torch::nn::ConvTranspose2dImpl::ConvTranspose2dImpl;
///
///   torch::Tensor forward(const torch::Tensor& input) {
///     return torch::nn::ConvTranspose2dImpl::forward(input, c10::nullopt);
///   }
/// };
///
/// TORCH_MODULE(ConvTranspose2dWrapper);
///
/// torch::nn::Sequential sequential(
///   ConvTranspose2dWrapper(torch::nn::ConvTranspose2dOptions(3, 3, 4));
/// ```
class TORCH_API ConvTranspose2dImpl : public ConvTransposeNdImpl<2, ConvTranspose2dImpl> {
 public:
  ConvTranspose2dImpl(
      int64_t input_channels,
      int64_t output_channels,
      ExpandingArray<2> kernel_size)
      : ConvTranspose2dImpl(ConvTranspose2dOptions(input_channels, output_channels, kernel_size)) {
  }
  explicit ConvTranspose2dImpl(ConvTranspose2dOptions options_);
  Tensor forward(const Tensor& input,
                 const c10::optional<at::IntArrayRef>& output_size = c10::nullopt);
};

TORCH_MODULE(ConvTranspose2d);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ConvTranspose3d ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/// Applies the ConvTranspose3d function.
/// See https://pytorch.org/docs/master/nn.html#torch.nn.ConvTranspose3d to
/// learn about the exact behavior of this module.
///
/// NOTE: `ConvTranspose3d` currently cannot be used in a `Sequential` module,
/// because `Sequential` module doesn't support modules with forward method that takes
/// optional arguments. Users should create their own wrapper for `ConvTranspose3d`
/// and have its forward method just accept a tensor, if they want to use it in a
/// `Sequential` module.
///
/// Example:
/// ```
/// struct ConvTranspose3dWrapperImpl : public torch::nn::ConvTranspose3dImpl {
///   using torch::nn::ConvTranspose3dImpl::ConvTranspose3dImpl;
///
///   torch::Tensor forward(const torch::Tensor& input) {
///     return torch::nn::ConvTranspose3dImpl::forward(input, c10::nullopt);
///   }
/// };
///
/// TORCH_MODULE(ConvTranspose3dWrapper);
///
/// torch::nn::Sequential sequential(
///   ConvTranspose3dWrapper(torch::nn::ConvTranspose3dOptions(3, 3, 4));
/// ```
class TORCH_API ConvTranspose3dImpl : public ConvTransposeNdImpl<3, ConvTranspose3dImpl> {
 public:
  ConvTranspose3dImpl(
      int64_t input_channels,
      int64_t output_channels,
      ExpandingArray<3> kernel_size)
      : ConvTranspose3dImpl(ConvTranspose3dOptions(input_channels, output_channels, kernel_size)) {
  }
  explicit ConvTranspose3dImpl(ConvTranspose3dOptions options_);
  Tensor forward(const Tensor& input,
                 const c10::optional<at::IntArrayRef>& output_size = c10::nullopt);
};

TORCH_MODULE(ConvTranspose3d);

} // namespace nn
} // namespace torch