# ![Stanford DiffDART](http://www.diffdart.org/assets/images/logos.svg)

[![Build Status](https://dev.azure.com/keenonwerling/diffdart/_apis/build/status/keenon.diffdart?branchName=master)](https://dev.azure.com/keenonwerling/diffdart/_build/latest?definitionId=1&branchName=master)

# Stanford DiffDART

`pip3 install diffdart`

** WARNING: PRE-ALPHA SOFTWARE! EXPECT BUGS! **

Use physics as a non-linearity in your neural network! We've got a forward pass, which is a single physics timestep:

![Forward pass illustration](http://www.diffdart.org/assets/images/data-flow-fwd.svg)

And an analytical backwards pass, that works even through contact and friction!

![Backpropagation illustration](http://www.diffdart.org/assets/images/data-flow-back.svg)

It's as easy as:

```python
from diffdart import dart_layer

# Everything is a PyTorch Tensor, and this is differentiable!!
next_positions, next_velocities = dart_layer(world, positions, velocities, forces)
```

This is a fork of the popular DART physics engine, with analytical gradients and a PyTorch binding.

Check out our [website](http://www.diffdart.org) for more information.
