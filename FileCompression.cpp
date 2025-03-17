// FileCompression.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <queue>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <bitset>
#include <sstream>

using namespace std;

const string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

string encodeBase64(const string& binaryStr) {
    string encoded = "";
    int val = 0, bits = 0;
    for (char c : binaryStr) {
        val = (val << 1) | (c - '0');
        bits++;
        if (bits == 6) {
            encoded += BASE64_CHARS[val];
            val = 0;
            bits = 0;
        }
    }
    if (bits) {
        val <<= (6 - bits);
        encoded += BASE64_CHARS[val];
    }
    return encoded;
}

string decodeBase64(const string& encodedStr) {
    unordered_map<char, int> base64_map;
    for (int i = 0; i < 64; ++i) base64_map[BASE64_CHARS[i]] = i;

    string binaryStr = "";
    for (char c : encodedStr) {
        int val = base64_map[c];
        for (int i = 5; i >= 0; --i) {
            binaryStr += ((val >> i) & 1) ? '1' : '0';
        }
    }
    return binaryStr;
}

struct HuffmanNode {
    char data;
    int freq;
    HuffmanNode* left, * right;
    HuffmanNode(char d, int f) : data(d), freq(f), left(nullptr), right(nullptr) {}
};

struct Compare {
    bool operator()(HuffmanNode* l, HuffmanNode* r) {
        return l->freq > r->freq;
    }
};

unordered_map<char, string> huffmanCodes;
unordered_map<string, char> reverseHuffmanCodes;
mutex mtx;

void generateCodes(HuffmanNode* root, string str) {
    if (!root) return;
    if (root->data != '\0') {
        lock_guard<mutex> lock(mtx);
        huffmanCodes[root->data] = str;
        reverseHuffmanCodes[str] = root->data;
    }
    generateCodes(root->left, str + "0");
    generateCodes(root->right, str + "1");
}

void buildHuffmanTree(const unordered_map<char, int>& freqMap) {
    priority_queue<HuffmanNode*, vector<HuffmanNode*>, Compare> minHeap;
    for (auto pair : freqMap)
        minHeap.push(new HuffmanNode(pair.first, pair.second));

    while (minHeap.size() != 1) {
        HuffmanNode* left = minHeap.top(); minHeap.pop();
        HuffmanNode* right = minHeap.top(); minHeap.pop();
        HuffmanNode* merged = new HuffmanNode('\0', left->freq + right->freq);
        merged->left = left;
        merged->right = right;
        minHeap.push(merged);
    }

    thread t1(generateCodes, minHeap.top(), "");
    t1.join();
}

void compressFile(const string& inputFile, const string& outputFile) {
    ifstream in(inputFile, ios::binary);
    if (!in) {
        cerr << "Error opening input file!" << endl;
        return;
    }

    unordered_map<char, int> freqMap;
    char ch;
    while (in.get(ch)) freqMap[ch]++;
    in.clear();
    in.seekg(0);

    buildHuffmanTree(freqMap);

    ofstream out(outputFile);
    out << freqMap.size() << "\n";
    for (auto pair : freqMap) {
        out << pair.first << " " << pair.second << "\n";
    }

    string encodedStr = "";
    while (in.get(ch)) encodedStr += huffmanCodes[ch];
    while (encodedStr.size() % 6 != 0) encodedStr += "0";
    out << encodeBase64(encodedStr);

    in.close();
    out.close();
}

void decompressFile(const string& inputFile, const string& outputFile) {
    ifstream in(inputFile);
    if (!in) {
        cerr << "Error opening compressed file!" << endl;
        return;
    }

    unordered_map<char, int> freqMap;
    int mapSize;
    in >> mapSize;
    in.ignore();

    for (int i = 0; i < mapSize; ++i) {
        char c;
        int freq;
        in.get(c);
        in >> freq;
        in.ignore();
        freqMap[c] = freq;
    }

    buildHuffmanTree(freqMap);

    string encodedBase64, encodedBits;
    getline(in, encodedBase64);
    encodedBits = decodeBase64(encodedBase64);
    in.close();

    ofstream out(outputFile);
    string temp = "";
    for (char bit : encodedBits) {
        temp += bit;
        if (reverseHuffmanCodes.find(temp) != reverseHuffmanCodes.end()) {
            out.put(reverseHuffmanCodes[temp]);
            temp = "";
        }
    }
    out.close();
}

int main() {
    string inputFile = "input.txt";
    string compressedFile = "compressed.txt";
    string decompressedFile = "output.txt";

    thread compressionThread(compressFile, inputFile, compressedFile);
    compressionThread.join();

    cout << "File compressed successfully!" << endl;

    thread decompressionThread(decompressFile, compressedFile, decompressedFile);
    decompressionThread.join();

    cout << "File decompressed successfully!" << endl;
    return 0;
}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
