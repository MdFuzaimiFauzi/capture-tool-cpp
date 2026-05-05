#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QDateTime>
#include <QCoreApplication>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <chrono>

using namespace std;
using namespace cv;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("Capture Tool");
    window.resize(800, 600);

    QLabel *feedLabel = new QLabel(&window);
    feedLabel->setMinimumSize(640, 480);
    feedLabel->setAlignment(Qt::AlignCenter);
    feedLabel->setStyleSheet("background-color: black; color: white;");
    feedLabel->setText("Starting camera initialization...");

    QLabel *statusLabel = new QLabel("Ready", &window);

    QSpinBox *countdownBox = new QSpinBox(&window);
    countdownBox->setRange(0, 15);
    countdownBox->setPrefix("Countdown: ");
    countdownBox->setSuffix(" seconds");
    countdownBox->setValue(3);

    QSpinBox *durationBox = new QSpinBox(&window);
    durationBox->setRange(1, 120);
    durationBox->setPrefix("Duration: ");
    durationBox->setSuffix(" seconds");
    durationBox->setValue(5);

    QPushButton *captureBtn = new QPushButton("Capture Single Frame", &window);
    QPushButton *recordBtn = new QPushButton("Record Dataset Video", &window);

    QHBoxLayout *controlsLayout = new QHBoxLayout;
    controlsLayout->addWidget(countdownBox);
    controlsLayout->addWidget(durationBox);
    controlsLayout->addWidget(captureBtn);
    controlsLayout->addWidget(recordBtn);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(feedLabel);
    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(statusLabel);
    window.setLayout(mainLayout);

    string pipeline = "libcamerasrc ! video/x-raw, width=1280, height=720, framerate=30/1, format=RGBx ! videoconvert ! video/x-raw, format=BGR ! appsink drop=true sync=false";
    VideoCapture cap(pipeline, CAP_GSTREAMER);
    
    if (!cap.isOpened()) {
        statusLabel->setText("GStreamer pipeline failed. Falling back to native V4L2...");
        cap.open(0, CAP_V4L2);
    }

    if (!cap.isOpened()) {
        feedLabel->setText("Fatal Error: Camera module not accessible.");
        return -1;
    }

    QTimer *timer = new QTimer(&window);
    VideoWriter writer;
    bool isRecording = false;
    int targetFrames = 0;
    int framesRecorded = 0;

    QObject::connect(timer, &QTimer::timeout, [&]() {
        Mat frame;
        cap.read(frame);
        if (frame.empty()) return;

        if (isRecording) {
            writer.write(frame);
            framesRecorded++;
            statusLabel->setText(QString("Recording active: %1 / %2 frames").arg(framesRecorded).arg(targetFrames));
            
            if (framesRecorded >= targetFrames) {
                isRecording = false;
                writer.release();
                recordBtn->setEnabled(true);
                captureBtn->setEnabled(true);
                statusLabel->setText("Dataset video successfully saved to this folder.");
            }
        }

        Mat rgbFrame;
        cvtColor(frame, rgbFrame, COLOR_BGR2RGB);
        QImage img(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
        feedLabel->setPixmap(QPixmap::fromImage(img).scaled(feedLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
    
    timer->start(33); 

    QObject::connect(captureBtn, &QPushButton::clicked, [&]() {
        Mat frame;
        cap.read(frame);
        if (!frame.empty()) {
            QString filename = "frame_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".jpg";
            imwrite(filename.toStdString(), frame);
            statusLabel->setText("Captured and saved " + filename + " to this folder.");
        }
    });

    QObject::connect(recordBtn, &QPushButton::clicked, [&]() {
        recordBtn->setEnabled(false);
        captureBtn->setEnabled(false);
        int countdown = countdownBox->value();
        int duration = durationBox->value();

        for (int i = countdown; i > 0; i--) {
            statusLabel->setText(QString("Recording begins in %1 seconds...").arg(i));
            QCoreApplication::processEvents();
            this_thread::sleep_for(chrono::seconds(1));
        }

        targetFrames = duration * 30; 
        framesRecorded = 0;
        QString filename = "video_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".avi";
        
        writer.open(filename.toStdString(), VideoWriter::fourcc('M','J','P','G'), 30, Size(cap.get(CAP_PROP_FRAME_WIDTH), cap.get(CAP_PROP_FRAME_HEIGHT)));

        if (writer.isOpened()) {
            isRecording = true;
            statusLabel->setText("Recording initiated.");
        } else {
            statusLabel->setText("Error creating the video file payload.");
            recordBtn->setEnabled(true);
            captureBtn->setEnabled(true);
        }
    });

    window.show();
    return app.exec();
}
