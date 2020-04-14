#include <cstdint>
#include <fstream>
#include <chrono>
#include <random>

const unsigned int START_ADDRESS = 0x200;
const unsigned int FONTSET_START_ADDRESS = 0x50;
const unsigned int FONTSET_SIZE = 80;
const unsigned int VIDEO_HEIGHT = 32;
const unsigned int VIDEO_WIDTH = 64;

//Sprites for characters
uint8_t fontset[FONTSET_SIZE] =
{
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

class Chip8 {

  public:

    //Components of CHIP-8
    uint8_t registers[16];
    uint8_t memory[4096];
    uint16_t index;
    uint16_t pc;
    uint16_t stack[16];
    uint8_t sp;
    uint8_t delayTimer;
    uint8_t soundTimer;
    uint8_t keypad[16];
    uint32_t video[VIDEO_WIDTH * VIDEO_HEIGHT];
    uint16_t opcode;

    //Helper member variables
    std::default_random_engine randGen;
    std::uniform_int_distribution<uint8_t> randByte;

    //Constructor
    Chip8() : randGen(std::chrono::system_clock::now().time_since_epoch().count())
    {

      // Initialize PC
      pc = START_ADDRESS;

      // Load fonts into memory
      for (unsigned int i = 0; i < FONTSET_SIZE; ++i)
      {
        memory[FONTSET_START_ADDRESS + i] = fontset[i];
      }

      // Initialize RNG
      randByte = std::uniform_int_distribution<uint8_t>(0, 255U);

    }

    void LoadROM(char const* filename) {
      // Open the file as a stream of binary and move the file pointer to the end
      std::ifstream file(filename, std::ios::binary | std::ios::ate);

      if (file.is_open())
      {
        // Get size of file and allocate a buffer to hold the contents
        std::streampos size = file.tellg();
        char* buffer = new char[size];

        // Go back to the beginning of the file and fill the buffer
        file.seekg(0, std::ios::beg);
        file.read(buffer, size);
        file.close();

        // Load the ROM contents into the Chip8's memory, starting at 0x200
        for (long i = 0; i < size; ++i)
        {
          memory[START_ADDRESS + i] = buffer[i];
        }

        // Free the buffer
        delete[] buffer;
      }
    }
    
    
    /**
     * 00E0: CLS
     * Clears the display.
     */
    void OP_00E0() {
      memset(video, 0, VIDEO_HEIGHT * VIDEO_WIDTH);
    }
    
    /**
     * 00EE: RET
     * Returns from a subroutine.
     */
    void OP_00EE() {
      --sp;
      pc = stack[sp];
    }

    /**
     * 1nnn: JP addr
     * Jumps to location nnn.
     */
    void OP_1nnn() {
      uint16_t address = opcode & 0x0FFFu; //bitmask to get location
      pc = address;
    }

    /**
     * 2nnn: CALL addr
     * Calls subroutine at location nnn.
     */
    void OP_2nnn() {
      uint16_t address = opcode & 0x0FFFu;
      pc = address;
      stack[sp] = pc;
      ++sp;
      pc = address;
    }

    /**
     * 3xkk: SE Vx, byte
     * Skips next instruction if Vx = kk.
     */
    void OP_3xkk() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t byte = opcode & 0x00FFu;
      if (registers[Vx] == byte) {
        pc += 2;
      }
    }

    /**
     * 4xkk: SNE Vx, byte
     * Skips next instruction if Vx != kk.
     */
    void OP_4xkk() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t byte = opcode & 0x00FFu;
      if (registers[Vx] != byte) {
        pc += 2;
      }
    }

    /**
     * 5xy0: SE Vx, Vy
     * Skips next instruction if Vx = Vy.
     */
    void OP_5xy0() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      if (registers[Vx] == registers[Vy]) {
        pc += 2;
      }
    }

    /**
     * 6xnn: LD Vx, nn
     * Loads 8-bit number nn into register Vx.
     */
    void OP_6xnn() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t byte = opcode & 0x00FFu;
      registers[Vx] = byte;
    }

    /**
     * 7xnn: ADD Vx, nn
     * Adds number nn to register Vx.
     */
    void OP_7xnn() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t byte = opcode & 0x00FFu;
      registers[Vx] += byte;
    }

    /**
     * 8xy0: LD Vx, Vy
     * Sets register Vx with the value of register Vy.
     */
    void OP_8xy0() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      registers[Vx] = registers[Vy];
    }

    /**
     * 8xy1: OR Vx, Vy
     * Logical OR values in registers Vx and Vy and stores
     * result in Vx.
     */
    void OP_8xy1() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      registers[Vx] |= registers[Vy];
    }

    /**
     * 8xy2: AND Vx, Vy
     * Logical AND values in registers Vx and Vy and stores
     * result in Vx.
     */
    void OP_8xy2() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      registers[Vx] &= registers[Vy];
    } 

    /**
     * 8xy3: XOR Vx, Vy
     * Logical XOR values in registers Vx and Vy and stores
     * result in Vx.
     */
    void OP_8xy3() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      registers[Vx] ^= registers[Vy];
    }

    /**
     * 8xy4: ADD Vx, Vy
     * Adds values of registers Vx and Vy and stores them in Vx.
     * If the result is more than 8 bits, register VF (16) is 
     * set to 1, otherwise 0.
     */
    void OP_8xy4() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      uint16_t result = registers[Vx] + registers[Vy];
      if (result > 255u){
        registers[15] = 1;
      } else {
        registers[15] = 0;
      }
      registers[Vx] += registers[Vy];
      //registers[Vx] = result & 0xFFu;
    }

    /**
     * 8xy4: SUB Vx, Vy
     * Subtracts value of registers Vy from Vx and stores the result in Vx.
     * If value of Vx is more than Vy, register VF (16) is 
     * set to 1, otherwise 0.
     */
    void OP_8xy5() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      if (registers[Vx] > registers[Vy]){
        registers[15] = 1;
      } else {
        registers[15] = 0;
      }
      registers[Vx] -= registers[Vy];
    }

    /**
     * 8xy6: SHR Vx, Vy
     * Store value of register Vy shifted right by one bit in register
     * Vx. Set register VF to value of LSB of Vy before the shift.
     */
    void OP_8xy6() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      registers[15] = (registers[Vy] & 0x1u);
      registers[Vx] = (registers[Vy]>>1);
    } 

    /**
     * 8xy7: SUBN Vx, Vy
     * Subtracts value of registers Vx from Vy and stores the result in Vx.
     * If value of Vy is more than Vx, register VF (16) is 
     * set to 1, otherwise 0.
     */
    void OP_8xy7() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      if (registers[Vy] > registers[Vx]){
        registers[15] = 1;
      } else {
        registers[15] = 0;
      }
      registers[Vx] = registers[Vy] - registers[Vx];
    }

    /**
     * 8xyE: SHL Vx, Vy
     * Store value of register Vy shifted left by one bit in register
     * Vx. Set register VF to value of MSB of Vy before the shift.
     */
    void OP_8xyE() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      registers[15] = (registers[Vy] & 0xF0u) >> 7u;
      registers[Vx] = (registers[Vy]<<1);
    } 

    /**
     * 9xy0: SNE Vx, Vy
     * Skips next instruction if Vx != Vy.
     */
    void OP_9xy0() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      if (registers[Vx] != registers[Vy]) {
        pc += 2;
      }
    }

    /**
     * Annn: LD I, addr
     * Set index register I as location nnn.
     */
    void OP_Annn() {
      uint16_t address = opcode & 0x0FFFu;
      index = address;
    }

    /**
     * Bnnn: JP V0, addr
     * Jumps to location nnn with offset stipulated by value of register V0.
     */
    void OP_Bnnn() {
      uint16_t address = opcode & 0x0FFFu;
      pc = address + registers[0];
    }

    /**
     * Cxkk: RND Vx, byte
     * Set value of register Vx as a random 8-bit number ANDed with
     * number kk.
     */
    void OP_Cxkk() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t byte = (opcode & 0x00FFu);
      registers[Vx] = (randByte(randGen) & byte);
    }
    
    /**
     * Dxyn: DRW Vx, Vy, nibble
     * Draw sprite at position Vx, Vy, with n bytes of sprite data,
     * starting at the address stored in the index register I. 
     * Sets register VF to 1 if any set pixels are change to unset,
     * 0 otherwise.
     */
    void OP_Dxyn() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t Vy = (opcode & 0x00F0u) >> 4u;
      uint8_t height = opcode & 0x000Fu;

      uint8_t xPos = registers[Vx] % VIDEO_WIDTH;
      uint8_t yPos = registers[Vy] % VIDEO_HEIGHT;

      registers[15] = 0;

      for (unsigned int row = 0; row < height; row++) {
        uint8_t spriteByte = memory[index + row];

        for (unsigned int col = 0; col < 8; col++) {
          uint8_t spritePixel = spriteByte & (0x80u >> col);
          uint32_t* screenPixel = &video[(yPos + row) * VIDEO_WIDTH];

          if (spritePixel) { //Sprite pixel is set
            if (*screenPixel == 0xFFFFFFFF) { //Screen pixel also set - collision
              registers[15] = 1;
            }
            
          } 

          //XOR with sprite pixel since it is now set
          *screenPixel ^= 0xFFFFFFFF;

        }
      }
    }

     /**
     * Ex9E: SKP Vx
     * Skip next instruction if key with the value of Vx is pressed.
     */
    void OP_Ex9E() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t key = registers[Vx];
      if (keypad[key]) {
        pc += 2;
      }
    }

    /**
     * ExA1: SKNP VX
     * Skip next instruction if key with the value of Vx is not pressed.
     */
    void OP_ExA1() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t key = registers[Vx];
      if (!keypad[key]) {
        pc += 2;
      }
    }

    /**
     * Fx07: LD Vx, DT
     * Set Vx = delay timer value
     */
    void OP_Fx07() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      registers[Vx] = delayTimer;
    }
    
    /**
     * Fx0A: LD Vx, K
     * Wait for a key press, store the value of the key in Vx.
     */
    void OP_Fx0A() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;

      if (keypad[0]) {
        registers[Vx] = 0;
      } else if (keypad[1]) {
        registers[Vx] = 1;
      } else if (keypad[2]) {
        registers[Vx] = 2;
      } else if (keypad[3]) {
        registers[Vx] = 3;
      } else if (keypad[4]) {
        registers[Vx] = 4;
      } else if (keypad[5]) {
        registers[Vx] = 5;
      } else if (keypad[6]) {
        registers[Vx] = 6;
      } else if (keypad[7]) {
        registers[Vx] = 7;
      } else if (keypad[8]) {
        registers[Vx] = 8;
      } else if (keypad[9]) {
        registers[Vx] = 9;
      } else if (keypad[10]) {
        registers[Vx] = 10;
      } else if (keypad[11]) {
        registers[Vx] = 11;
      } else if (keypad[12]) {
        registers[Vx] = 12;
      } else if (keypad[13]) {
        registers[Vx] = 13;
      } else if (keypad[14]) {
        registers[Vx] = 14;
      } else if (keypad[15]) {
        registers[Vx] = 15;
      } else {
        pc -= 2;
      }
    }

    /**
     * Fx15: LD DT, Vx
     * Set delay timer = Vx.
     */
    void OP_Fx15() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      delayTimer = registers[Vx];
    }

    /**
     * Fx18: LD ST, Vx
     * Set sound timer = Vx.
     */
    void OP_Fx18() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      soundTimer = registers[Vx];
    }  

    /**
     * Fx1E: ADD I, Vx
     * Set I = I + Vx.
     */
    void OP_Fx1E() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      index += registers[Vx];
    }

    /**
     * Fx29: LD F, Vx
     * Set I = location of sprite for digit Vx
     */
    void OP_Fx29() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t digit = registers[Vx];
      index += FONTSET_START_ADDRESS + (5 * digit);
    }

    /**
     * Fx33: LD B, Vx
     * Stores BCD representation of Vx in memory locations
     * I, I+1 and I+2.
     * The interpreter takes the decimal value of Vx, and places
     * the hundreds digit at I, tens digit at I+1 and ones digit
     * at I+2.
     */
    void OP_Fx33() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      uint8_t value = registers[Vx];
      
      memory[index+2] = value % 10;
      value /= 10;
      memory[index+1] = value % 10;
      value /= 10;
      memory[index] = value % 10;
    }

    /**
     * Fx55: LD [I], Vx
     * Stores registers V0 through Vx in memory starting at 
     * location I.
     */
    void OP_Fx55() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      for (int reg = 0; reg <= Vx; reg++) {
          uint8_t value = registers[reg];
          memory[index + reg] = value;
      }
    }

    /**
     * Fx65: LD Vx, [I]
     * Read registers V0 through Vx from memory starting at 
     * location I.
     */
    void OP_Fx65() {
      uint8_t Vx = (opcode & 0x0F00u) >> 8u;
      for (int reg = 0; reg <= Vx; reg++) {
          registers[reg] = memory[index + reg];
      }
    }
};
