#include "neural_network.h"
#include "model_weights.h"
float hidden_layer[HIDDEN_SIZE];
float output_layer[OUTPUT_SIZE];
int predict_class(float* input_data)
{
    //Dense layer    
    for (int i = 0; i <HIDDEN_SIZE ; i++)
    {
        hidden_layer[i]=dense1_biases[i];
        for (int j = 0; j <INPUT_SIZE; j++)
        {
            hidden_layer[i]+=input_data[j] * dense1_weights[(j*HIDDEN_SIZE)+i];
        }
        //ReLU
        if (hidden_layer[i] < 0.0f) {
                hidden_layer[i] = 0.0f; 
            }
    }
    //dense layer2
    for (int x  = 0; x < OUTPUT_SIZE; x++)
    {
        output_layer[x]=dense2_biases[x];
        for (int y = 0; y < HIDDEN_SIZE; y++)
        {
            output_layer[x]+=hidden_layer[y] * dense2_weights[(y*OUTPUT_SIZE)+x];
        }
        
    }

    int max=0;
    float max_value=output_layer[0];
    for(int z = 1; z < OUTPUT_SIZE; z++)
    {
        if(output_layer[z]>max_value)
        {
            max_value=output_layer[z];
            max=z;
        }
    }
    
    
   return max;
}




