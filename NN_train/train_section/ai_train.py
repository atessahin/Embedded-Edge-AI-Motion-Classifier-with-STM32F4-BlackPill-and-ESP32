import numpy as np

X = np.load('X_data.npy').astype(np.float32)
y = np.load('y_tags.npy')

ornek_sayisi = X.shape[0]
input_size = 540 
neurons = 16
output_size = 3

np.random.seed(42)

# (inputs * weights) + biases inputs -> dense -> relu
class Layer_Dense:
    def __init__(self, n_inputs, n_neurons):
        # init small random weights
        self.weights = 0.10 * np.random.randn(n_inputs, n_neurons).astype(np.float32)
        # init zero biases
        self.biases = np.zeros((1, n_neurons), dtype=np.float32)

    def forward(self, inputs):
        self.inputs = inputs
        self.output = np.dot(inputs, self.weights) + self.biases

    def backward(self, dvalue):
        # calc gradients for backprop
        self.dweights = np.dot(self.inputs.T, dvalue)
        self.dbiases = np.sum(dvalue, axis=0, keepdims=True)
        self.dinputs = np.dot(dvalue, self.weights.T)

# turns negative numbers to 0 dense -> relu -> dense
class Activation_ReLu:
    def forward(self, inputs):
        self.inputs = inputs
        self.output = np.maximum(0, inputs)

    def backward(self, dvalues):
        self.dinputs = dvalues.copy()
        self.dinputs[self.inputs <= 0] = 0

#converts raw outputs to probabilities dense -> softmax -> loss
class Activation_Softmax:
    def forward(self, inputs):
        # stabilize softmax to prevent overflow
        exp_values = np.exp(inputs - np.max(inputs, axis=1, keepdims=True))
        # normalize to probabilities
        probabilities = exp_values / np.sum(exp_values, axis=1, keepdims=True)
        self.output = probabilities

# base loss class to calculate mean error
class Loss:
    def calculate(self, output, y):
        sample_losses = self.forward(output, y)
        data_loss = np.mean(sample_losses)
        return data_loss
    
#measures error between predictions and true labels
class Loss_CategoricalCrossentropy(Loss):
    def forward(self, y_pred, y_true):
        samples = len(y_pred)
        # clip to avoid log(0)
        y_pred_clipped = np.clip(y_pred, 1e-7, 1 - 1e-7)
        
        # handle 1d or 2d labels
        if len(y_true.shape) == 1:
            correct_confidences = y_pred_clipped[range(samples), y_true]
        elif len(y_true.shape) == 2:
            correct_confidences = np.sum(y_pred_clipped * y_true, axis=1)

        negative_log_likelihoods = -np.log(correct_confidences)
        return negative_log_likelihoods

# combined softmax and loss for faster/stable backprop dense -> combined -> gradients
class Activation_Softmax_Loss_CategoricalCrossentropy:
    def __init__(self):
        self.activation = Activation_Softmax()
        self.loss = Loss_CategoricalCrossentropy()

    def forward(self, inputs, y_true):
        self.activation.forward(inputs)
        self.output = self.activation.output
        return self.loss.calculate(self.output, y_true)
    
    def backward(self, dvalue, y_true):
        samples = len(dvalue)

        # convert 2d one-hot to 1d
        if len(y_true.shape) == 2:
            y_true = np.argmax(y_true, axis=1)

        # simplified gradient calc
        self.dinputs = dvalue.copy()     
        self.dinputs[range(samples), y_true] -= 1
        self.dinputs = self.dinputs / samples

# updates weights and biases using gradients
class Optimizer:
    def __init__(self, learning_rate=1.0):
        self.learning_rate = learning_rate

    def update_params(self, layer):
        layer.weights += -self.learning_rate * layer.dweights
        layer.biases += -self.learning_rate * layer.dbiases

dense1 = Layer_Dense(input_size, neurons)
activation1 = Activation_ReLu()
dense2 = Layer_Dense(neurons, output_size)

loss_activation = Activation_Softmax_Loss_CategoricalCrossentropy()
optimizer = Optimizer(learning_rate=0.001)
epochs = 10001

# training loop
for epoch in range(epochs):
    # forward pass
    # input -> dense1
    dense1.forward(X)
    # dense1 -> relu
    activation1.forward(dense1.output)
    # relu -> dense2
    dense2.forward(activation1.output)

    # dense2 -> softmax & loss
    loss = loss_activation.forward(dense2.output, y)
    
    # calculate accuracy
    prediction = np.argmax(loss_activation.output, axis=1)
    y_labels = np.argmax(y, axis=1) if len(y.shape) == 2 else y
    accuracy = np.mean(prediction == y_labels)

    if epoch % 500 == 0 or epoch == epochs - 1:
        print(f"Epoch: {epoch}, Loss: {loss:.3f}, Accuracy: {accuracy:.3f}")

    # backward pass 
    # loss -> softmax -> dense2 -> relu -> dense1
    loss_activation.backward(loss_activation.output, y)
    dense2.backward(loss_activation.dinputs)
    activation1.backward(dense2.dinputs)
    dense1.backward(activation1.dinputs)

    #  update weights
    optimizer.update_params(dense1)
    optimizer.update_params(dense2)

# esp32 .h creator
def c_array(name, arr):
    flat = arr.astype(np.float32).reshape(-1)
    values = ", ".join(f"{v:.6f}f" for v in flat)
    return f"const float {name}[{len(flat)}] = {{{values}}};\n"

def export_model(path, W1, b1, W2, b2):
    with open(path, "w", encoding="utf-8") as f:
        f.write("#ifndef MODEL_WEIGHTS_H\n")
        f.write("#define MODEL_WEIGHTS_H\n\n")
        f.write("// generated by ai_train.py (oop version)\n")
        f.write("// inference preprocessing must be per-window/per-axis relative normalization.\n\n")
        f.write(c_array("dense1_weights", W1))
        f.write("\n")
        f.write(c_array("dense1_biases", b1))
        f.write("\n")
        f.write(c_array("dense2_weights", W2))
        f.write("\n")
        f.write(c_array("dense2_biases", b2))
        f.write("\n#endif\n")

export_model("model_weights.h", dense1.weights, dense1.biases, dense2.weights, dense2.biases)
print("\nmodel_weights.h successfully created. you can directly include this file in your hardware project.")