/*MIT License

Copyright (c) 2016 MTA SZTAKI

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filewritestream.h"
#include "ApeOgreRenderPlugin.h"

Ape::OgreRenderPlugin::OgreRenderPlugin()
{
	mpScene = Ape::IScene::getSingletonPtr();
	mpSystemConfig = Ape::ISystemConfig::getSingletonPtr();
	mpMainWindow = Ape::IMainWindow::getSingletonPtr();
	mEventDoubleQueue = Ape::DoubleQueue<Event>();
	mpEventManager = Ape::IEventManager::getSingletonPtr();
	mpEventManager->connectEvent(Ape::Event::Group::NODE, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::LIGHT, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::CAMERA, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::GEOMETRY_FILE, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::GEOMETRY_TEXT, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::GEOMETRY_PLANE, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::GEOMETRY_BOX, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::GEOMETRY_CYLINDER, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::GEOMETRY_CONE, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::GEOMETRY_TUBE, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::GEOMETRY_SPHERE, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::GEOMETRY_TORUS, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::MATERIAL_FILE, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::MATERIAL_MANUAL, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::PASS_PBS, std::bind(&OgreRenderPlugin::eventCallBack, this, std::placeholders::_1));
	mpRoot = NULL;
	mpSceneMgr = NULL;
	mRenderWindows = std::map<std::string, Ogre::RenderWindow*>();
	mpOverlaySys = NULL;
	mpOgreMovableTextFactory = NULL;
	mpOverlayMgr = NULL;
	mpOverlay = NULL;
	mpOverlayContainer = NULL;
	mpOverlayTextArea = NULL;
	mpOverlayFontManager = NULL;
	mpOverlayFont = NULL;
	mpHlmsPbsManager = NULL;
	mpShaderGenerator = NULL;
	mpShaderGeneratorResolver = NULL;
	mOgreRenderWindowConfigList = Ape::OgreRenderWindowConfigList();
	mOgreCameras = std::vector<Ogre::Camera*>();
	mPbsMaterials = std::map<std::string, Ogre::PbsMaterial*>();
}

Ape::OgreRenderPlugin::~OgreRenderPlugin()
{
	std::cout << "OgreRenderPlugin dtor" << std::endl;
	delete mpRoot;
}

void Ape::OgreRenderPlugin::eventCallBack(const Ape::Event& event)
{
	mEventDoubleQueue.push(event);
}

void Ape::OgreRenderPlugin::processEventDoubleQueue()
{
	mEventDoubleQueue.swap();
	while (!mEventDoubleQueue.emptyPop())
	{
		Ape::Event event = mEventDoubleQueue.front();
		if (event.group == Ape::Event::Group::NODE)
		{
			if (auto node = mpScene->getNode(event.subjectName).lock())
			{
				std::string nodeName = node->getName();
				if (event.type == Ape::Event::Type::NODE_CREATE)
					mpSceneMgr->getRootSceneNode()->createChildSceneNode(nodeName);
				else 
				{
					Ogre::SceneNode* ogreNode = nullptr;
					if (mpSceneMgr->hasSceneNode(nodeName))
						ogreNode = mpSceneMgr->getSceneNode(nodeName);
					if (ogreNode)
					{
						switch (event.type)
						{
						case Ape::Event::Type::NODE_PARENTNODE:
						{
							if (auto parentNode = node->getParentNode().lock())
							{
								Ogre::SceneNode* ogreParentNode = ogreNode->getParentSceneNode();
								if (ogreParentNode)
									ogreParentNode->removeChild(ogreNode);
								if (mpSceneMgr->hasSceneNode(parentNode->getName()))
								{
									ogreParentNode = mpSceneMgr->getSceneNode(parentNode->getName());
									ogreParentNode->addChild(ogreNode);
								}
							}
						}
							break;
						case Ape::Event::Type::NODE_DELETE:
							;
							break;
						case Ape::Event::Type::NODE_POSITION:
							ogreNode->setPosition(ConversionToOgre(node->getPosition()));
							break;
						case Ape::Event::Type::NODE_ORIENTATION:
							ogreNode->setOrientation(ConversionToOgre(node->getOrientation()));
							break;
						}
					}
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_FILE)
		{
			if (auto geometryFile = std::static_pointer_cast<Ape::IFileGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = geometryFile->getName();
				std::string parentNodeName = "";
				if (auto parentNode = geometryFile->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_FILE_CREATE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_FILE_PARENTNODE:
				{
					if (auto ogreGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_FILE_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_FILE_FILENAME:
				{
					std::string fileExtension = geometryName.substr(geometryName.find_first_of("."));
					if (fileExtension == ".mesh")
						mpSceneMgr->createEntity(geometryName, geometryName);
				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_PLANE)
		{
			if (auto primitive = std::static_pointer_cast<Ape::IPlaneGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = primitive->getName();
				std::string parentNodeName = "";
				if (auto parentNode = primitive->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_PLANE_CREATE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_PLANE_PARENTNODE:
				{
					if (auto ogreGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_PLANE_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_PLANE_MATERIAL:
				{
					if (auto ogrePrimitveGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto material = primitive->getMaterial().lock())
						{
							auto ogreMaterial = Ogre::MaterialManager::getSingleton().getByName(material->getName());
							ogrePrimitveGeometry->setMaterial(ogreMaterial);
							if (auto pass = material->getPass().lock())
							{
								if (auto ogrePbsMaterial = mPbsMaterials[pass->getName()])
								{
									size_t ogreSubEntitxCount = ogrePrimitveGeometry->getNumSubEntities();
									for (size_t i = 0; i < ogreSubEntitxCount; i++)
									{
										Ogre::SubEntity* ogreSubEntity = ogrePrimitveGeometry->getSubEntity(i);
										mpHlmsPbsManager->bind(ogreSubEntity, ogrePbsMaterial, pass->getName());
									}
								}
							}
						}
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_PLANE_PARAMETERS:
				{
					Ape::GeometryPlaneParameters parameters = primitive->getParameters();
					Procedural::PlaneGenerator().setNumSegX(parameters.numSeg.x).setNumSegY(parameters.numSeg.y).setSizeX(parameters.size.x).setSizeY(parameters.size.y)
						.setUTile(parameters.tile.x).setVTile(parameters.tile.y).realizeMesh(geometryName);
					mpSceneMgr->createEntity(geometryName, geometryName);
					
				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_BOX)
		{
			if (auto primitive = std::static_pointer_cast<Ape::IBoxGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = primitive->getName();
				std::string parentNodeName = "";
				if (auto parentNode = primitive->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_BOX_CREATE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_BOX_PARENTNODE:
				{
					if (auto ogreGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_BOX_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_BOX_MATERIAL:
				{
					if (auto ogrePrimitveGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto material = primitive->getMaterial().lock())
						{
							auto ogreMaterial = Ogre::MaterialManager::getSingleton().getByName(material->getName());
							ogrePrimitveGeometry->setMaterial(ogreMaterial);
							if (auto pass = material->getPass().lock())
							{
								if (auto ogrePbsMaterial = mPbsMaterials[pass->getName()])
								{
									size_t ogreSubEntitxCount = ogrePrimitveGeometry->getNumSubEntities();
									for (size_t i = 0; i < ogreSubEntitxCount; i++)
									{
										Ogre::SubEntity* ogreSubEntity = ogrePrimitveGeometry->getSubEntity(i);
										mpHlmsPbsManager->bind(ogreSubEntity, ogrePbsMaterial, pass->getName());
									}
								}
							}
						}
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_BOX_PARAMETERS:
				{
					Ape::GeometryBoxParameters parameters = primitive->getParameters();
					Procedural::BoxGenerator().setSizeX(parameters.dimensions.x).setSizeY(parameters.dimensions.x).setSizeZ(parameters.dimensions.x)
							.realizeMesh(geometryName);
					mpSceneMgr->createEntity(geometryName, geometryName);

				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_SPHERE)
		{
			if (auto primitive = std::static_pointer_cast<Ape::ISphereGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = primitive->getName();
				std::string parentNodeName = "";
				if (auto parentNode = primitive->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_SPHERE_CREATE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_SPHERE_PARENTNODE:
				{
					if (auto ogreGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_SPHERE_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_SPHERE_MATERIAL:
				{
					if (auto ogrePrimitveGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto material = primitive->getMaterial().lock())
						{
							auto ogreMaterial = Ogre::MaterialManager::getSingleton().getByName(material->getName());
							ogrePrimitveGeometry->setMaterial(ogreMaterial);
							if (auto pass = material->getPass().lock())
							{
								if (auto ogrePbsMaterial = mPbsMaterials[pass->getName()])
								{
									size_t ogreSubEntitxCount = ogrePrimitveGeometry->getNumSubEntities();
									for (size_t i = 0; i < ogreSubEntitxCount; i++)
									{
										Ogre::SubEntity* ogreSubEntity = ogrePrimitveGeometry->getSubEntity(i);
										mpHlmsPbsManager->bind(ogreSubEntity, ogrePbsMaterial, pass->getName());
									}
								}
							}
						}
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_SPHERE_PARAMETERS:
				{
					Ape::GeometrySphereParameters parameters = primitive->getParameters();
					Procedural::SphereGenerator().setRadius(parameters.radius)
						.setUTile(parameters.tile.x).setVTile(parameters.tile.y)
						.realizeMesh(geometryName);
					mpSceneMgr->createEntity(geometryName, geometryName);

				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_CYLINDER)
		{
			if (auto primitive = std::static_pointer_cast<Ape::ICylinderGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = primitive->getName();
				std::string parentNodeName = "";
				if (auto parentNode = primitive->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_CYLINDER_CREATE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_CYLINDER_PARENTNODE:
				{
					if (auto ogreGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_CYLINDER_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_CYLINDER_MATERIAL:
				{
					if (auto ogrePrimitveGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto material = primitive->getMaterial().lock())
						{
							auto ogreMaterial = Ogre::MaterialManager::getSingleton().getByName(material->getName());
							ogrePrimitveGeometry->setMaterial(ogreMaterial);
							if (auto pass = material->getPass().lock())
							{
								if (auto ogrePbsMaterial = mPbsMaterials[pass->getName()])
								{
									size_t ogreSubEntitxCount = ogrePrimitveGeometry->getNumSubEntities();
									for (size_t i = 0; i < ogreSubEntitxCount; i++)
									{
										Ogre::SubEntity* ogreSubEntity = ogrePrimitveGeometry->getSubEntity(i);
										mpHlmsPbsManager->bind(ogreSubEntity, ogrePbsMaterial, pass->getName());
									}
								}
							}
						}
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_CYLINDER_PARAMETERS:
				{
					Ape::GeometryCylinderParameters parameters = primitive->getParameters();
					Procedural::CylinderGenerator().setHeight(parameters.height)
						.setRadius(parameters.radius)
						.setUTile(parameters.tile)
						.realizeMesh(geometryName);
					mpSceneMgr->createEntity(geometryName, geometryName);

				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_TORUS)
		{
			if (auto primitive = std::static_pointer_cast<Ape::ITorusGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = primitive->getName();
				std::string parentNodeName = "";
				if (auto parentNode = primitive->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_TORUS_CREATE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_TORUS_PARENTNODE:
				{
					if (auto ogreGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_TORUS_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_TORUS_MATERIAL:
				{
					if (auto ogrePrimitveGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto material = primitive->getMaterial().lock())
						{
							auto ogreMaterial = Ogre::MaterialManager::getSingleton().getByName(material->getName());
							ogrePrimitveGeometry->setMaterial(ogreMaterial);
							if (auto pass = material->getPass().lock())
							{
								if (auto ogrePbsMaterial = mPbsMaterials[pass->getName()])
								{
									size_t ogreSubEntitxCount = ogrePrimitveGeometry->getNumSubEntities();
									for (size_t i = 0; i < ogreSubEntitxCount; i++)
									{
										Ogre::SubEntity* ogreSubEntity = ogrePrimitveGeometry->getSubEntity(i);
										mpHlmsPbsManager->bind(ogreSubEntity, ogrePbsMaterial, pass->getName());
									}
								}
							}
						}
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_TORUS_PARAMETERS:
				{
					Ape::GeometryTorusParameters parameters = primitive->getParameters();
					Procedural::TorusGenerator().setRadius(parameters.radius)
						.setSectionRadius(parameters.sectionRadius)
						.setUTile(parameters.tile.x).setVTile(parameters.tile.y)
						.realizeMesh(geometryName);
					mpSceneMgr->createEntity(geometryName, geometryName);

				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_CONE)
		{
			if (auto primitive = std::static_pointer_cast<Ape::IConeGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = primitive->getName();
				std::string parentNodeName = "";
				if (auto parentNode = primitive->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_CONE_CREATE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_CONE_PARENTNODE:
				{
					if (auto ogreGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_CONE_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_CONE_MATERIAL:
				{
					if (auto ogrePrimitveGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto material = primitive->getMaterial().lock())
						{
							auto ogreMaterial = Ogre::MaterialManager::getSingleton().getByName(material->getName());
							ogrePrimitveGeometry->setMaterial(ogreMaterial);
							if (auto pass = material->getPass().lock())
							{
								if (auto ogrePbsMaterial = mPbsMaterials[pass->getName()])
								{
									size_t ogreSubEntitxCount = ogrePrimitveGeometry->getNumSubEntities();
									for (size_t i = 0; i < ogreSubEntitxCount; i++)
									{
										Ogre::SubEntity* ogreSubEntity = ogrePrimitveGeometry->getSubEntity(i);
										mpHlmsPbsManager->bind(ogreSubEntity, ogrePbsMaterial, pass->getName());
									}
								}
							}
						}
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_CONE_PARAMETERS:
				{
					Ape::GeometryConeParameters parameters = primitive->getParameters();
					Procedural::ConeGenerator().setRadius(parameters.radius)
						.setHeight(parameters.height)
						.setNumSegBase(parameters.numSeg.x).setNumSegHeight(parameters.numSeg.y)
						.setUTile(parameters.tile)
						.realizeMesh(geometryName);
					mpSceneMgr->createEntity(geometryName, geometryName);

				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_TUBE)
		{
			if (auto primitive = std::static_pointer_cast<Ape::ITubeGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = primitive->getName();
				std::string parentNodeName = "";
				if (auto parentNode = primitive->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_TUBE_CREATE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_TUBE_PARENTNODE:
				{
					if (auto ogreGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_TUBE_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_TUBE_MATERIAL:
				{
					if (auto ogrePrimitveGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto material = primitive->getMaterial().lock())
						{
							auto ogreMaterial = Ogre::MaterialManager::getSingleton().getByName(material->getName());
							ogrePrimitveGeometry->setMaterial(ogreMaterial);
							if (auto pass = material->getPass().lock())
							{
								if (auto ogrePbsMaterial = mPbsMaterials[pass->getName()])
								{
									size_t ogreSubEntitxCount = ogrePrimitveGeometry->getNumSubEntities();
									for (size_t i = 0; i < ogreSubEntitxCount; i++)
									{
										Ogre::SubEntity* ogreSubEntity = ogrePrimitveGeometry->getSubEntity(i);
										mpHlmsPbsManager->bind(ogreSubEntity, ogrePbsMaterial, pass->getName());
									}
								}
							}
						}
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_TUBE_PARAMETERS:
				{
					Ape::GeometryTubeParameters parameters = primitive->getParameters();
					Procedural::TubeGenerator().setHeight(parameters.height)
						.setUTile(parameters.tile)
						.realizeMesh(geometryName);
					mpSceneMgr->createEntity(geometryName, geometryName);

				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_TEXT)
		{
			if (auto geometryText = std::static_pointer_cast<Ape::ITextGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = geometryText->getName();
				std::string parentNodeName = "";
				if (auto parentNode = geometryText->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_TEXT_CREATE:
				{
					if (auto ogreText = (Ape::OgreMovableText*)mpSceneMgr->createMovableObject(geometryName, "MovableText"))
					{
						ogreText->setTextAlignment(Ape::OgreMovableText::H_CENTER, Ape::OgreMovableText::V_ABOVE);
						ogreText->showOnTop(true);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_TEXT_PARENTNODE:
				{
					if (auto ogreTextGeometry = (Ape::OgreMovableText*)mpSceneMgr->getMovableObject(geometryName, "MovableText"))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreTextGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_TEXT_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_TEXT_OFFSET:
				{
					if (auto ogreText = (Ape::OgreMovableText*)mpSceneMgr->getMovableObject(geometryName, "MovableText"))
					{
						if (auto textGeometry = std::static_pointer_cast<Ape::ITextGeometry>(mpScene->getEntity(geometryName).lock()))
							ogreText->setLocalTranslation(Ape::ConversionToOgre(textGeometry->getOffset()));
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_TEXT_CAPTION:
				{
					if (auto ogreText = (Ape::OgreMovableText*)mpSceneMgr->getMovableObject(geometryName, "MovableText"))
					{
						if (auto textGeometry = std::static_pointer_cast<Ape::ITextGeometry>(mpScene->getEntity(geometryName).lock()))
							ogreText->setCaption(textGeometry->getCaption());
					}
				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::GEOMETRY_MANUAL)
		{
			if (auto geometryManual = std::static_pointer_cast<Ape::IManualGeometry>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string geometryName = geometryManual->getName();
				std::string parentNodeName = "";
				if (auto parentNode = geometryManual->getParentNode().lock())
					parentNodeName = parentNode->getName();
				switch (event.type)
				{
				case Ape::Event::Type::GEOMETRY_MANUAL_CREATE:
				{
					mpSceneMgr->createManualObject(geometryName);
				}
					break;
				case Ape::Event::Type::GEOMETRY_MANUAL_PARENTNODE:
				{
					if (auto ogreGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNodeName))
							ogreParentNode->attachObject(ogreGeometry);
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_MANUAL_DELETE:
					;
					break;
				case Ape::Event::Type::GEOMETRY_MANUAL_MATERIAL:
				{
					if (auto ogreManualGeometry = mpSceneMgr->getEntity(geometryName))
					{
						if (auto material = geometryManual->getMaterial().lock())
						{
							auto ogreMaterial = Ogre::MaterialManager::getSingleton().getByName(material->getName());
							ogreManualGeometry->setMaterial(ogreMaterial);
							if (auto pass = material->getPass().lock())
							{
								if (auto ogrePbsMaterial = mPbsMaterials[pass->getName()])
								{
									size_t ogreSubEntitxCount = ogreManualGeometry->getNumSubEntities();
									for (size_t i = 0; i < ogreSubEntitxCount; i++)
									{
										Ogre::SubEntity* ogreSubEntity = ogreManualGeometry->getSubEntity(i);
										mpHlmsPbsManager->bind(ogreSubEntity, ogrePbsMaterial, pass->getName());
									}
								}
							}
						}
					}
				}
					break;
				case Ape::Event::Type::GEOMETRY_MANUAL_PARAMETER:
				{
					if (auto ogreManual = mpSceneMgr->getManualObject(geometryName))
					{
						if (geometryManual->getOperationType() == Ape::Geometry::OperationType::TRIANGLELIST)
						{
							if (auto material = geometryManual->getMaterial().lock())
							{
								Ape::ManualGeometryParameter parameter = geometryManual->getParameter();
								ogreManual->begin(material->getName(), ConversionToOgre(Ape::Geometry::OperationType::TRIANGLELIST));
								for (int i = 0; i < parameter.vertexList.size(); i++)
								{
									ogreManual->position(parameter.vertexList[i].x, parameter.vertexList[i].y, parameter.vertexList[i].z);
									if (parameter.normalList.size() != 0 && i < parameter.normalList.size())
										ogreManual->normal(parameter.normalList[i].x, parameter.normalList[i].y, parameter.normalList[i].z);
									if (parameter.colorList.size() != 0 && i < parameter.colorList.size())
										ogreManual->colour(parameter.colorList[i].x, parameter.colorList[i].y, parameter.colorList[i].z);
									if (parameter.textureCoordList.size() != 0 && i < parameter.textureCoordList.size())
										ogreManual->textureCoord(parameter.textureCoordList[i].x, 1.0f - parameter.textureCoordList[i].y);
								}
								for (int i = 0; i < parameter.indexList.size(); i++)
									ogreManual->index(parameter.indexList[i]);
								for (int i = 0; i < parameter.triangleList.size(); i++)
									ogreManual->triangle(parameter.triangleList[i].x, parameter.triangleList[i].y, parameter.triangleList[i].z);
								ogreManual->end();
							}
						}
					}
				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::MATERIAL_FILE)
		{
			if (auto materialFile = std::static_pointer_cast<Ape::IFileMaterial>(mpScene->getEntity(event.subjectName).lock()))
			{
				switch (event.type)
				{
				case Ape::Event::Type::MATERIAL_FILE_CREATE:
					;
					break;
				case Ape::Event::Type::MATERIAL_FILE_DELETE:
					;
					break;
				case Ape::Event::Type::MATERIAL_FILE_FILENAME:
					;
					break;
				case Ape::Event::Type::MATERIAL_FILE_SETASSKYBOX:
				{
					std::string skyBoxMaterialName = materialFile->getName();
					if (Ogre::MaterialManager::getSingleton().resourceExists(skyBoxMaterialName))
						mpSceneMgr->setSkyBox(true, skyBoxMaterialName);
				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::MATERIAL_MANUAL)
		{
			if (auto materialManual = std::static_pointer_cast<Ape::IManualMaterial>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string materialName = materialManual->getName();
				auto result = Ogre::MaterialManager::getSingleton().createOrRetrieve(materialName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
				Ogre::MaterialPtr ogreMaterial = result.first.staticCast<Ogre::Material>();
				switch (event.type)
				{
				case Ape::Event::Type::MATERIAL_MANUAL_CREATE:
					;
					break;
				case Ape::Event::Type::MATERIAL_MANUAL_DELETE:
					;
					break;
				case Ape::Event::Type::MATERIAL_MANUAL_PASS:
				{
					if (auto pass = materialManual->getPass().lock())
					{
						Ogre::MaterialManager::getSingleton().remove(materialName);
						auto ogrePassMaterial = Ogre::MaterialManager::getSingleton().getByName(pass->getName());
						if (!ogrePassMaterial.isNull())
							ogrePassMaterial->clone(materialName);
					}
				}
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::PASS_PBS)
		{
			if (auto passPbs = std::static_pointer_cast<Ape::IPbsPass>(mpScene->getEntity(event.subjectName).lock()))
			{
				std::string passPbsName = passPbs->getName();
				auto result = Ogre::MaterialManager::getSingleton().createOrRetrieve(passPbsName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
				Ogre::MaterialPtr ogrePbsPassMaterial = result.first.staticCast<Ogre::Material>();
				if (!ogrePbsPassMaterial.isNull())
				{
					switch (event.type)
					{
					case Ape::Event::Type::PASS_PBS_CREATE:
					{
						ogrePbsPassMaterial->createTechnique()->createPass();
						Ogre::PbsMaterial* ogrePbsMaterial = new Ogre::PbsMaterial();
						mPbsMaterials[passPbsName] = ogrePbsMaterial;
					}
						break;
					case Ape::Event::Type::PASS_PBS_AMBIENT:
						ogrePbsPassMaterial->setAmbient(ConversionToOgre(passPbs->getAmbientColor()));
						break;
					case Ape::Event::Type::PASS_PBS_DIFFUSE:
						ogrePbsPassMaterial->setDiffuse(ConversionToOgre(passPbs->getDiffuseColor()));
						break;
					case Ape::Event::Type::PASS_PBS_EMISSIVE:
						ogrePbsPassMaterial->setSelfIllumination(ConversionToOgre(passPbs->getEmissiveColor()));
						break;
					case Ape::Event::Type::PASS_PBS_SPECULAR:
						ogrePbsPassMaterial->setSpecular(ConversionToOgre(passPbs->getSpecularColor()));
						break;
					case Ape::Event::Type::PASS_PBS_SHININESS:
						ogrePbsPassMaterial->setShininess(passPbs->getShininess());
						break;
					case Ape::Event::Type::PASS_PBS_ALBEDO:
						mPbsMaterials[passPbsName]->setAlbedo(ConversionToOgre(passPbs->getAlbedo()));
						break;
					case Ape::Event::Type::PASS_PBS_F0:
						mPbsMaterials[passPbsName]->setF0(ConversionToOgre(passPbs->getF0()));
						break;
					case Ape::Event::Type::PASS_PBS_ROUGHNESS:
						mPbsMaterials[passPbsName]->setRoughness(passPbs->getRoughness());
						break;
					case Ape::Event::Type::PASS_PBS_LIGHTROUGHNESSOFFSET:
						mPbsMaterials[passPbsName]->setLightRoughnessOffset(passPbs->getLightRoughnessOffset());
						break;
					case Ape::Event::Type::PASS_PBS_DELETE:
						;
						break;
					}
				}
			}
		}
		else if (event.group == Ape::Event::Group::LIGHT)
		{
			if (auto light = std::static_pointer_cast<Ape::ILight>(mpScene->getEntity(event.subjectName).lock()))
			{
				Ogre::Light* ogreLight = nullptr;
				if (mpSceneMgr->hasLight(light->getName()))
					ogreLight = mpSceneMgr->getLight(light->getName());
				switch (event.type)
				{
				case Ape::Event::Type::LIGHT_CREATE:
					mpSceneMgr->createLight(light->getName());
					break;
				case Ape::Event::Type::LIGHT_ATTENUATION:
					ogreLight->setAttenuation(light->getLightAttenuation().range, light->getLightAttenuation().constant, light->getLightAttenuation().linear, light->getLightAttenuation().quadratic);
					break;
				case Ape::Event::Type::LIGHT_DIFFUSE:
					ogreLight->setDiffuseColour(Ape::ConversionToOgre(light->getDiffuseColor()));
					break;
				case Ape::Event::Type::LIGHT_DIRECTION:
					ogreLight->setDirection(Ape::ConversionToOgre(light->getLightDirection()));
					break;
				case Ape::Event::Type::LIGHT_SPECULAR:
					ogreLight->setSpecularColour(Ape::ConversionToOgre(light->getSpecularColor()));
					break;
				case Ape::Event::Type::LIGHT_SPOTRANGE:
					ogreLight->setSpotlightRange(Ogre::Radian(light->getLightSpotRange().innerAngle.toRadian()), Ogre::Radian(light->getLightSpotRange().outerAngle.toRadian()), light->getLightSpotRange().falloff);
					break;
				case Ape::Event::Type::LIGHT_TYPE:
					ogreLight->setType(Ape::ConversionToOgre(light->getLightType()));
					break;
				case Ape::Event::Type::LIGHT_PARENTNODE:
					{
						if (auto parentNode = light->getParentNode().lock())
						{
							if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNode->getName()))
								ogreParentNode->attachObject(ogreLight);
						}
					}
					break;
				case Ape::Event::Type::LIGHT_DELETE:
					;
					break;
				}
			}
		}
		else if (event.group == Ape::Event::Group::CAMERA)
		{
			if (auto camera = std::static_pointer_cast<Ape::ICamera>(mpScene->getEntity(event.subjectName).lock()))
			{
				switch (event.type)
				{
				case Ape::Event::Type::CAMERA_CREATE:
				{
					if (auto ogreCamera = mpSceneMgr->createCamera(event.subjectName))
					{
						if (auto viewPort = mRenderWindows[event.subjectName.c_str()]->addViewport(ogreCamera))
						{
							//TODO why it is working instead of in the init phase?
							ogreCamera->setAspectRatio(Ogre::Real(viewPort->getActualWidth()) / Ogre::Real(viewPort->getActualHeight()));
							mOgreCameras.push_back(ogreCamera);
							if (mOgreCameras.size() == 1)
							{
								if (Ogre::RTShader::ShaderGenerator::initialize())
								{
									mpShaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
									mpShaderGenerator->addSceneManager(mpSceneMgr);
									mpShaderGeneratorResolver = new Ape::ShaderGeneratorResolver(mpShaderGenerator);
									Ogre::MaterialManager::getSingleton().addListener(mpShaderGeneratorResolver);
									viewPort->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
									Ogre::RTShader::RenderState* pMainRenderState = mpShaderGenerator->createOrRetrieveRenderState(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME).first;
									pMainRenderState->reset();
									pMainRenderState->addTemplateSubRenderState(mpShaderGenerator->createSubRenderState(Ogre::RTShader::PerPixelLighting::Type));
									mpShaderGenerator->invalidateScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
								}
								else
									std::cout << "Problem in the RTSS init" << std::endl;
							}
						}
					}
				}
					break;
				case Ape::Event::Type::CAMERA_PARENTNODE:
				{
					if (auto ogreCamera = mpSceneMgr->getCamera(camera->getName()))
					{
						if (auto parentNode = camera->getParentNode().lock())
						{
							if (auto ogreParentNode = mpSceneMgr->getSceneNode(parentNode->getName()))
								ogreParentNode->attachObject(ogreCamera);
						}
					}
				}
					break;
				case Ape::Event::Type::CAMERA_DELETE:
					;
					break;
				case Ape::Event::Type::CAMERA_FOCALLENGTH:
				{
					if (mpSceneMgr->hasCamera(event.subjectName))
						mpSceneMgr->getCamera(event.subjectName)->setFocalLength(camera->getFocalLength());
				}
					break;
				case Ape::Event::Type::CAMERA_ASPECTRATIO:
				{
					if (mpSceneMgr->hasCamera(event.subjectName))
						mpSceneMgr->getCamera(event.subjectName)->setAspectRatio(camera->getAspectRatio());
				}
					break;
				case Ape::Event::Type::CAMERA_FOVY:
				{
					if (mpSceneMgr->hasCamera(event.subjectName))
						mpSceneMgr->getCamera(event.subjectName)->setFOVy(ConversionToOgre(camera->getFOVy()));
				}
					break;
				case Ape::Event::Type::CAMERA_FRUSTUMOFFSET:
				{
					if (mpSceneMgr->hasCamera(event.subjectName))
						mpSceneMgr->getCamera(event.subjectName)->setFrustumOffset(Ape::ConversionToOgre(camera->getFrustumOffset()));
				}
					break;
				case Ape::Event::Type::CAMERA_FARCLIP:
				{
					if (mpSceneMgr->hasCamera(event.subjectName))
						mpSceneMgr->getCamera(event.subjectName)->setFarClipDistance(camera->getFarClipDistance());
				}
					break;
				case Ape::Event::Type::CAMERA_NEARCLIP:
				{
					if (mpSceneMgr->hasCamera(event.subjectName))
						mpSceneMgr->getCamera(event.subjectName)->setNearClipDistance(camera->getNearClipDistance());
				}
					break;
				case Ape::Event::Type::CAMERA_POSITIONOFFSET:
				{
					if (mpSceneMgr->hasCamera(event.subjectName))
						mpSceneMgr->getCamera(event.subjectName)->setPosition(ConversionToOgre(camera->getPositionOffset()));
				}
					break;
				case Ape::Event::Type::CAMERA_ORIENTATIONOFFSET:
				{
					if (mpSceneMgr->hasCamera(event.subjectName))
						mpSceneMgr->getCamera(event.subjectName)->setOrientation(ConversionToOgre(camera->getOrientationOffset()));
				}
					break;
				}
			}
		}
		mEventDoubleQueue.pop();
	}
}

bool Ape::OgreRenderPlugin::frameStarted( const Ogre::FrameEvent& evt )
{
	return Ogre::FrameListener::frameStarted( evt );
}

bool Ape::OgreRenderPlugin::frameRenderingQueued( const Ogre::FrameEvent& evt )
{
	//std::stringstream ss;
	//ss << mRenderWindows[0]->getLastFPS();
	//TODO overlay
	//mpOverlayTextArea->setCaption(ss.str());

	processEventDoubleQueue();
	
	return Ogre::FrameListener::frameRenderingQueued( evt );
}

bool Ape::OgreRenderPlugin::frameEnded( const Ogre::FrameEvent& evt )
{
	return Ogre::FrameListener::frameEnded( evt );
}

void Ape::OgreRenderPlugin::Stop()
{
	
}

void Ape::OgreRenderPlugin::Suspend()
{
	
}

void Ape::OgreRenderPlugin::Restart()
{
	
}

void Ape::OgreRenderPlugin::Run()
{
	try
	{
        mpRoot->renderOneFrame();
        mpRoot->startRendering();
	}
	catch (const Ogre::RenderingAPIException& ex)
	{
		std::cout << ex.getFullDescription() << std::endl;
	}
	catch (const Ogre::Exception& ex)
	{
		std::cout << ex.getFullDescription() << std::endl;
	}
}

void Ape::OgreRenderPlugin::Step()
{
	try
	{
		mpRoot->renderOneFrame();
#ifndef __APPLE__
		Ogre::WindowEventUtilities::messagePump();
#endif
	}
	catch (const Ogre::RenderingAPIException& ex)
	{
		std::cout << ex.getFullDescription() << std::endl;
	}
	catch (const Ogre::Exception& ex)
	{
		std::cout << ex.getFullDescription() << std::endl;
	}
}

void Ape::OgreRenderPlugin::Init()
{
	std::string renderSystemName;
	std::stringstream fileFullPath;
	fileFullPath << mpSystemConfig->getFolderPath() << "\\ApeOgreRenderPlugin.json";
	FILE* apeOgreRenderPluginConfigFile = std::fopen(fileFullPath.str().c_str(), "r");
	char readBuffer[65536];
	if (apeOgreRenderPluginConfigFile)
	{
		rapidjson::FileReadStream jsonFileReaderStream(apeOgreRenderPluginConfigFile, readBuffer, sizeof(readBuffer));
		rapidjson::Document jsonDocument;
		jsonDocument.ParseStream(jsonFileReaderStream);
		if (jsonDocument.IsObject())
		{
			rapidjson::Value& renderSystem = jsonDocument["renderSystem"];
			renderSystemName = renderSystem.GetString();
			rapidjson::Value& renderWindows = jsonDocument["renderWindows"];
			for (auto& renderWindow : renderWindows.GetArray())
			{
				Ape::OgreRenderWindowConfig ogreRenderWindowConfig;
				for (rapidjson::Value::MemberIterator renderWindowMemberIterator = 
					renderWindow.MemberBegin(); renderWindowMemberIterator != renderWindow.MemberEnd(); ++renderWindowMemberIterator)
				{
					if (renderWindowMemberIterator->name == "monitorIndex")
						ogreRenderWindowConfig.monitorIndex = renderWindowMemberIterator->value.GetInt();
					else if (renderWindowMemberIterator->name == "resolution")
					{
						for (rapidjson::Value::MemberIterator resolutionMemberIterator = 
							renderWindow[renderWindowMemberIterator->name].MemberBegin();
							resolutionMemberIterator != renderWindow[renderWindowMemberIterator->name].MemberEnd(); ++resolutionMemberIterator)
						{
							if (resolutionMemberIterator->name == "width")
								ogreRenderWindowConfig.width = resolutionMemberIterator->value.GetInt();
							else if (resolutionMemberIterator->name == "height")
								ogreRenderWindowConfig.height = resolutionMemberIterator->value.GetInt();
							else if (resolutionMemberIterator->name == "fullScreen")
								ogreRenderWindowConfig.fullScreen = resolutionMemberIterator->value.GetBool();
						}
					}
					else if (renderWindowMemberIterator->name == "miscParams")
					{
						for (rapidjson::Value::MemberIterator miscParamsMemberIterator =
							renderWindow[renderWindowMemberIterator->name].MemberBegin();
							miscParamsMemberIterator != renderWindow[renderWindowMemberIterator->name].MemberEnd(); ++miscParamsMemberIterator)
						{
							if (miscParamsMemberIterator->name == "vSync")
								ogreRenderWindowConfig.vSync = miscParamsMemberIterator->value.GetBool();
							else if (miscParamsMemberIterator->name == "vSyncInterval")
								ogreRenderWindowConfig.vSyncInterval = miscParamsMemberIterator->value.GetInt();
							else if (miscParamsMemberIterator->name == "colorDepth")
								ogreRenderWindowConfig.colorDepth = miscParamsMemberIterator->value.GetInt();
							else if (miscParamsMemberIterator->name == "FSAA")
								ogreRenderWindowConfig.fsaa = miscParamsMemberIterator->value.GetInt();
							else if (miscParamsMemberIterator->name == "FSAAHint")
								ogreRenderWindowConfig.fsaaHint = miscParamsMemberIterator->value.GetString();
						}
					}
					else if (renderWindowMemberIterator->name == "viewports")
					{
						rapidjson::Value& viewports = renderWindow[renderWindowMemberIterator->name];
						for (auto& viewport : viewports.GetArray())
						{
							Ape::OgreViewPortConfig ogreViewPortConfig;
							for (rapidjson::Value::MemberIterator viewportMemberIterator =
								viewport.MemberBegin();
								viewportMemberIterator != viewport.MemberEnd(); ++viewportMemberIterator)
							{
								if (viewportMemberIterator->name == "zOrder")
									ogreViewPortConfig.zOrder = viewportMemberIterator->value.GetInt();
								else if (viewportMemberIterator->name == "left")
									ogreViewPortConfig.left = viewportMemberIterator->value.GetInt();
								else if (viewportMemberIterator->name == "top")
									ogreViewPortConfig.top = viewportMemberIterator->value.GetInt();
								else if (viewportMemberIterator->name == "width")
									ogreViewPortConfig.width = viewportMemberIterator->value.GetInt();
								else if (viewportMemberIterator->name == "height")
									ogreViewPortConfig.height = viewportMemberIterator->value.GetInt();
								else if (viewportMemberIterator->name == "camera")
								{
									for (rapidjson::Value::MemberIterator cameraMemberIterator =
										viewport[viewportMemberIterator->name].MemberBegin();
										cameraMemberIterator != viewport[viewportMemberIterator->name].MemberEnd(); ++cameraMemberIterator)
									{
										if (cameraMemberIterator->name == "nearClip")
											ogreViewPortConfig.camera.nearClip = cameraMemberIterator->value.GetFloat();
										else if (cameraMemberIterator->name == "farClip")
											ogreViewPortConfig.camera.farClip = cameraMemberIterator->value.GetFloat();
										else if (cameraMemberIterator->name == "fovY")
											ogreViewPortConfig.camera.fovY = cameraMemberIterator->value.GetFloat();
										else if (cameraMemberIterator->name == "positionOffset")
										{
											for (rapidjson::Value::MemberIterator elementMemberIterator =
												viewport[viewportMemberIterator->name][cameraMemberIterator->name].MemberBegin();
												elementMemberIterator != viewport[viewportMemberIterator->name][cameraMemberIterator->name].MemberEnd(); ++elementMemberIterator)
											{
												if (elementMemberIterator->name == "x")
													ogreViewPortConfig.camera.positionOffset.x = elementMemberIterator->value.GetFloat();
												else if (elementMemberIterator->name == "y")
													ogreViewPortConfig.camera.positionOffset.y = elementMemberIterator->value.GetFloat();
												else if (elementMemberIterator->name == "z")
													ogreViewPortConfig.camera.positionOffset.z = elementMemberIterator->value.GetFloat();
											}
										}
										else if (cameraMemberIterator->name == "orientationOffset")
										{
											Ogre::Quaternion orientationOffset;
											Ogre::Degree angle;
											Ogre::Vector3 axis;
											for (rapidjson::Value::MemberIterator elementMemberIterator =
												viewport[viewportMemberIterator->name][cameraMemberIterator->name].MemberBegin();
												elementMemberIterator != viewport[viewportMemberIterator->name][cameraMemberIterator->name].MemberEnd(); ++elementMemberIterator)
											{
												if (elementMemberIterator->name == "angle")
													angle = elementMemberIterator->value.GetFloat();
												else if (elementMemberIterator->name == "x")
													axis.x = elementMemberIterator->value.GetFloat();
												else if (elementMemberIterator->name == "y")
													axis.y = elementMemberIterator->value.GetFloat();
												else if (elementMemberIterator->name == "z")
													axis.z = elementMemberIterator->value.GetFloat();
											}
											orientationOffset.FromAngleAxis(angle, axis);
											ogreViewPortConfig.camera.orientationOffset = Ape::ConversionFromOgre(orientationOffset);
										}

									}
								}
							}
							ogreRenderWindowConfig.viewportList.push_back(ogreViewPortConfig);
						}
					}
				}
				mOgreRenderWindowConfigList.push_back(ogreRenderWindowConfig);
			}
		}
		fclose(apeOgreRenderPluginConfigFile);
	}	
	
	mpRoot = new Ogre::Root("", "", "ApeOgreRenderPlugin.log");
    
	Ogre::LogManager::getSingleton().createLog("ApeOgreRenderPlugin.log", true, false, false);

	#if defined (_DEBUG)
		Ogre::LogManager::getSingleton().setLogDetail(Ogre::LL_BOREME);
		if (renderSystemName == "DX11")
			mpRoot->loadPlugin( "RenderSystem_Direct3D11_d" );
		else if (renderSystemName == "DX9")
			mpRoot->loadPlugin("RenderSystem_Direct3D9_d");
		else 
			mpRoot->loadPlugin( "RenderSystem_GL_d" );
	#else
		Ogre::LogManager::getSingleton().setLogDetail(Ogre::LL_NORMAL);
		if (renderSystemName == "DX11")
			mpRoot->loadPlugin("RenderSystem_Direct3D11");
		else if (renderSystemName == "DX9")
			mpRoot->loadPlugin("RenderSystem_Direct3D9");
		else
			mpRoot->loadPlugin("RenderSystem_GL");
	#endif
    
	Ogre::RenderSystem* renderSystem = nullptr;
	if (renderSystemName == "DX11")
		renderSystem = mpRoot->getRenderSystemByName("Direct3D11 Rendering Subsystem");
	else if (renderSystemName == "DX9")
		renderSystem = mpRoot->getRenderSystemByName("Direct3D9 Rendering Subsystem");
	else
		renderSystem = mpRoot->getRenderSystemByName("OpenGL Rendering Subsystem");
	
    

	std::stringstream mediaFolder;
	mediaFolder << APE_SOURCE_DIR << "/plugins/ogreRender/media";

	mpRoot->setRenderSystem(renderSystem);

	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mediaFolder.str() + "/fonts",				 "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mediaFolder.str() + "/pbs", "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mediaFolder.str() + "/rtss", "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mediaFolder.str() + "/rtss/Cg", "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mediaFolder.str() + "/rtss/GLSL", "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mediaFolder.str() + "/rtss/GLSL150", "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mediaFolder.str() + "/rtss/GLSLES", "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mediaFolder.str() + "/rtss/HLSL", "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mediaFolder.str() + "/rtss/materials", "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(mpSystemConfig->getSceneSessionConfig().sessionResourceLocation, "FileSystem");
	
	mpRoot->initialise(false, "Ape");
	mpSceneMgr = mpRoot->createSceneManager(Ogre::ST_GENERIC);

	mpRoot->addFrameListener(this);

	Ogre::RenderWindowList renderWindowList;
	Ogre::RenderWindowDescriptionList winDescList;
	for (int i = 0; i < mOgreRenderWindowConfigList.size(); i++)
	{
		Ogre::RenderWindowDescription winDesc;
		std::stringstream ss;
		ss << mOgreRenderWindowConfigList[i].monitorIndex;
		winDesc.name = ss.str();
		winDesc.height = mOgreRenderWindowConfigList[i].height;
		winDesc.width = mOgreRenderWindowConfigList[i].width;
		winDesc.useFullScreen = mOgreRenderWindowConfigList[i].fullScreen;
		std::stringstream colourDepthSS;
		colourDepthSS << mOgreRenderWindowConfigList[i].colorDepth;
		winDesc.miscParams["colourDepth"] = colourDepthSS.str().c_str();
		winDesc.miscParams["vsync"] = mOgreRenderWindowConfigList[i].vSync ? "Yes" : "No";
		std::stringstream vsyncIntervalSS;
		vsyncIntervalSS << mOgreRenderWindowConfigList[i].vSyncInterval;
		winDesc.miscParams["vsyncInterval"] = vsyncIntervalSS.str().c_str();
		std::stringstream fsaaSS;
		fsaaSS << mOgreRenderWindowConfigList[i].fsaa;
		winDesc.miscParams["FSAA"] = fsaaSS.str().c_str();
		winDesc.miscParams["FSAAHint"] = mOgreRenderWindowConfigList[i].fsaaHint;
		std::stringstream monitorIndexSS;
		monitorIndexSS << mOgreRenderWindowConfigList[i].monitorIndex;
		winDesc.miscParams["monitorIndex"] = monitorIndexSS.str().c_str();
		/*winDesc.miscParams["Allow NVPersHUD"] = "No";
		winDesc.miscParams["Driver type"] = "Hardware";
		winDesc.miscParams["Information Queue Exceptions Bottom Level"] = "Info (exception on any message)";
		winDesc.miscParams["Max Requested Feature Levels"] = "11.0";
		winDesc.miscParams["Min Requested Feature Levels"] = "9.1";
		winDesc.miscParams["Floating-point mode"] = "Fastest";
		winDesc.miscParams["sRGB Gamma Conversion"] = "No";
		winDescList.push_back(winDesc);*/

		if (mpSystemConfig->getMainWindowConfig().creator == THIS_PLUGINNAME)
		{
			mRenderWindows[winDesc.name] = mpRoot->createRenderWindow(winDesc.name, winDesc.width, winDesc.height, winDesc.useFullScreen, &winDesc.miscParams);
			mRenderWindows[winDesc.name]->setDeactivateOnFocusChange(false);
			if (i == 0)
			{
				void* windowHnd = 0;
				mRenderWindows[winDesc.name]->getCustomAttribute("WINDOW", &windowHnd);
				std::ostringstream windowHndStr;
				windowHndStr << windowHnd;
				mOgreRenderWindowConfigList[0].windowHandler = windowHndStr.str();

				mpMainWindow->setHandle(windowHnd);
				mpMainWindow->setWidth(mOgreRenderWindowConfigList[0].width);
				mpMainWindow->setHeight(mOgreRenderWindowConfigList[0].height);
			}
			auto camera = std::static_pointer_cast<Ape::ICamera>(mpScene->createEntity(winDesc.name, Ape::Entity::Type::CAMERA).lock());
			if (camera)
			{
				//TODO why it is not ok
				//camera->setAspectRatio((float)mOgreRenderWindowConfigList[i].width / (float)mOgreRenderWindowConfigList[i].height);
				camera->setFocalLength(1.0f);
				camera->setNearClipDistance(mOgreRenderWindowConfigList[i].viewportList[0].camera.nearClip);
				camera->setFarClipDistance(mOgreRenderWindowConfigList[i].viewportList[0].camera.farClip);
				camera->setFOVy(mOgreRenderWindowConfigList[i].viewportList[0].camera.fovY.toRadian());
			}
		}
	}
	
	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

	mpOverlaySys = new Ogre::OverlaySystem();
	mpSceneMgr->addRenderQueueListener(mpOverlaySys);

	mpHlmsPbsManager = new Ogre::HlmsManager(mpSceneMgr);

	mpOgreMovableTextFactory = new Ape::OgreMovableTextFactory();
	mpRoot->addMovableObjectFactory(mpOgreMovableTextFactory);
}
